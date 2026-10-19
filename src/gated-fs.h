#ifndef GATED_FS_H
#define GATED_FS_H

// SD access while a podcast episode downloads in the background.
//
// The download task and the audio decoder share the card. One write can keep the card busy
// for up to 500 ms (sdWait() in the core's sd_diskio.cpp) while FATFS holds the volume lock,
// and a decoder blocked on that lock stops producing samples: only the I2S DMA ring (~186 ms)
// keeps the sound going. So the download task does every SD access under `sdGate`, and the
// audio path only tries it: when the card is busy a read returns 0 bytes, the library tries
// again on its next loop() and decodes from its input buffer (~400 ms at 128 kbit/s) meanwhile.
//
// The same file system also plays the episode while it is still being written. The audio
// library reads the file size once and FATFS freezes the size a reader sees at f_open, so a
// plain SD file would end after the first few seconds. GatedFile reports the final size
// instead, serves only what the downloader has flushed, and reopens the real file when its
// size snapshot runs out. If playback catches up with the download, read() returns 0 and the
// library waits silently until there is data again.
//
// Reading a file that another handle appends to is outside the FATFS contract (FF_FS_LOCK is
// 0, so nothing stops it). It is safe here because the writer only appends and the reader never
// reads past the size it saw at its own open. Never remove, rename or truncate the file while
// it is open.

#include <FS.h>
#include <FSImpl.h>
#include <SD.h>

#include <memory>

SemaphoreHandle_t sdGate = nullptr;

// Call once from setup(), before anything uses the gate.
void sdGateBegin() {
  if (!sdGate) sdGate = xSemaphoreCreateMutex();
}

bool sdGateTake(TickType_t wait = portMAX_DELAY) { return sdGate && xSemaphoreTake(sdGate, wait) == pdTRUE; }
void sdGateGive() { xSemaphoreGive(sdGate); }

// The episode the download task is writing. The task sets path and size before the first byte
// and then only moves `synced` forward; everything is read under growingMux.
struct GrowingFile {
  char path[200] = "";
  uint32_t synced = 0;    // bytes flushed to the card: safe to read
  uint32_t expected = 0;  // final size of the local file, 0 = not known
  bool done = false;      // no more data will come: complete, given up or cancelled
  bool hungry = false;    // a reader is waiting for data: flush early
};
GrowingFile growing;
portMUX_TYPE growingMux = portMUX_INITIALIZER_UNLOCKED;

void growingStart(const char *path, uint32_t synced, uint32_t expected) {
  portENTER_CRITICAL(&growingMux);
  strlcpy(growing.path, path, sizeof(growing.path));
  growing.synced = synced;
  growing.expected = expected;
  growing.done = false;
  growing.hungry = false;
  portEXIT_CRITICAL(&growingMux);
}

// No episode downloading (any more): no file is treated as growing.
void growingClear() {
  portENTER_CRITICAL(&growingMux);
  growing.path[0] = 0;
  portEXIT_CRITICAL(&growingMux);
}

void growingSynced(uint32_t synced) {
  portENTER_CRITICAL(&growingMux);
  growing.synced = synced;
  growing.hungry = false;
  portEXIT_CRITICAL(&growingMux);
}

// No more data: the file is complete or the download stopped. Readers then end at `finalSize`.
void growingDone(uint32_t finalSize) {
  portENTER_CRITICAL(&growingMux);
  growing.synced = finalSize;
  growing.expected = finalSize;
  growing.done = true;
  portEXIT_CRITICAL(&growingMux);
}

bool growingHungry() {
  portENTER_CRITICAL(&growingMux);
  bool h = growing.hungry;
  portEXIT_CRITICAL(&growingMux);
  return h;
}

// Copies the state if `path` is the file being downloaded.
bool growingGet(const char *path, uint32_t &synced, uint32_t &expected, bool &done, bool markHungry = false) {
  bool match;
  portENTER_CRITICAL(&growingMux);
  match = growing.path[0] && strcmp(growing.path, path) == 0;
  if (match) {
    synced = growing.synced;
    expected = growing.expected;
    done = growing.done;
    if (markHungry && !done) growing.hungry = true;
  }
  portEXIT_CRITICAL(&growingMux);
  return match;
}

// A read-only file for the audio library, see the comment at the top.
class GatedFile : public fs::FileImpl {
  static const uint32_t PROVISIONAL_SIZE = 0x7FFFFFFF;  // size while the server did not say
  static const uint32_t RESUME_LEAD = 32 * 1024;        // after starving, wait for this much data

  File _real;
  String _path;
  const char *_name;
  bool _growing;
  uint32_t _pos = 0;
  uint32_t _realSize = 0;  // size snapshot of _real
  uint32_t _lastExpected = 0;
  bool _needSeek = false;
  bool _hungry = false;

  // How far the file may be read now, and the size to report.
  void limits(uint32_t &readable, uint32_t &reported) {
    readable = reported = _realSize;
    if (!_growing) return;
    uint32_t synced, expected;
    bool done;
    if (!growingGet(_path.c_str(), synced, expected, done, false)) {
      // Another episode is downloading now; this one is whatever is on the card.
      readable = _realSize;
      reported = _lastExpected > _realSize ? _lastExpected : _realSize;
      return;
    }
    readable = synced;
    if (done) {
      reported = expected;
    } else {
      reported = expected ? expected : PROVISIONAL_SIZE;
    }
    _lastExpected = reported;
    if (_hungry && !done && synced < _pos + RESUME_LEAD) readable = _pos;  // keep waiting
  }

 public:
  GatedFile(File real, const String &path, bool growing) : _real(real), _path(path), _growing(growing) {
    _realSize = _real.size();
    int slash = _path.lastIndexOf('/');
    _name = _path.c_str() + (slash >= 0 ? slash + 1 : 0);
  }

  ~GatedFile() override { close(); }

  size_t read(uint8_t *buf, size_t len) override {
    if (!len) return 0;
    uint32_t readable, reported;
    limits(readable, reported);
    if (_pos >= readable) {
      if (_growing && _pos < reported) {
        uint32_t s, e;
        bool d;
        growingGet(_path.c_str(), s, e, d, true);  // ask the downloader to flush now
        _hungry = true;
      }
      return 0;
    }
    _hungry = false;
    // Bulk reads come from the decoder loop and must never block. The few single-byte reads of
    // a resume (frame sync search, nothing is playing then) may wait for the card.
    if (!sdGateTake(len >= 64 ? 0 : pdMS_TO_TICKS(1000))) return 0;
    if (!_real || _pos >= _realSize) {  // the size snapshot is used up: see what is there now
      if (_real) _real.close();
      _real = SD.open(_path, FILE_READ);
      _realSize = _real ? _real.size() : 0;
      _needSeek = true;
      if (!_real || _pos >= _realSize) {
        sdGateGive();
        return 0;
      }
    }
    if (_needSeek) {
      _real.seek(_pos);
      _needSeek = false;
    }
    size_t want = len;
    if (want > readable - _pos) want = readable - _pos;
    if (want > _realSize - _pos) want = _realSize - _pos;
    int got = _real.read(buf, want);
    sdGateGive();
    if (got <= 0) return 0;
    _pos += got;
    return got;
  }

  bool seek(uint32_t pos, SeekMode mode) override {
    uint32_t readable, reported;
    limits(readable, reported);
    if (mode == SeekCur) pos += _pos;
    if (mode == SeekEnd) pos += reported;
    _pos = pos;
    _needSeek = true;
    _hungry = false;
    return true;
  }

  size_t position() const override { return _pos; }

  size_t size() const override {
    uint32_t readable, reported;
    const_cast<GatedFile *>(this)->limits(readable, reported);
    return reported > _pos ? reported : _pos;  // the library subtracts position from this
  }

  void close() override {
    if (!_real) return;
    bool held = sdGateTake(pdMS_TO_TICKS(2000));  // giving a mutex we do not hold would assert
    _real.close();
    if (held) sdGateGive();
  }

  size_t write(const uint8_t *, size_t) override { return 0; }
  void flush() override {}
  bool setBufferSize(size_t) override { return true; }
  time_t getLastWrite() override { return 0; }
  const char *path() const override { return _path.c_str(); }
  const char *name() const override { return _name; }
  boolean isDirectory(void) override { return false; }
  fs::FileImplPtr openNextFile(const char *) override { return fs::FileImplPtr(); }
  boolean seekDir(long) override { return false; }
  String getNextFileName(void) override { return ""; }
  String getNextFileName(bool *) override { return ""; }
  void rewindDirectory(void) override {}
  operator bool() override { return (bool)_real; }
};

class GatedFSImpl : public fs::FSImpl {
 public:
  fs::FileImplPtr open(const char *path, const char *mode, const bool create) override {
    if (mode && mode[0] != 'r') return fs::FileImplPtr();  // the audio library only reads
    uint32_t synced, expected;
    bool done;
    bool isGrowing = growingGet(path, synced, expected, done);
    File real;
    if (sdGateTake(pdMS_TO_TICKS(2000))) {
      real = SD.open(path, FILE_READ);
      sdGateGive();
    }
    if (!real) return fs::FileImplPtr();
    return std::make_shared<GatedFile>(real, String(path), isGrowing);
  }

  bool exists(const char *path) override {
    uint32_t synced, expected;
    bool done;
    if (growingGet(path, synced, expected, done)) return true;  // no fopen and 4 kB buffer for it
    bool found = false;
    if (sdGateTake(pdMS_TO_TICKS(2000))) {
      found = SD.exists(path);
      sdGateGive();
    }
    return found;
  }

  bool rename(const char *, const char *) override { return false; }
  bool remove(const char *) override { return false; }
  bool mkdir(const char *) override { return false; }
  bool rmdir(const char *) override { return false; }
};

fs::FS gatedFS(std::make_shared<GatedFSImpl>());

#endif
