#ifndef PODCAST_H
#define PODCAST_H

// Podcast albums: fetch the newest episode of a feed into the album folder while it plays.
//
// Everything network-related runs on one task on core 0, away from the audio loop on core 1.
// The episode is written to the card in 4 kB blocks under sdGate and flushed regularly; the
// player reads the growing file through gatedFS (gated-fs.h).

#include <HTTPClient.h>
#include <MD5Builder.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <new>
#include <vector>

#include "ff.h"
#include "gated-fs.h"
#include "podcast-feed.h"
#include "wifi-setup.h"

#define PODCAST_USER_AGENT BOX_NAME " podcast player (ESP32)"
#define PODCAST_TASK_STACK 12288
#define PODCAST_WIFI_TIMEOUT_MS 10000
#define PODCAST_STALL_TIMEOUT_MS 20000
#define PODCAST_BLOCK 4096
#define PODCAST_FIRST_FLUSH (64 * 1024)  // playback can start once this much is on the card
#define PODCAST_FLUSH_EVERY (128 * 1024)
#define PODCAST_HUNGRY_FLUSH (16 * 1024)  // flush early by this much when a reader is waiting
// A connection that only trickles (Wi-Fi trouble leaves TCP crawling with its small window) is
// replaced: below 8 kB/s over 15 s, half the rate of a 128 kbit/s episode, playback would catch up.
// A write that fails after this much progress since the last failure is the SD driver's
// occasional false alarm (see writeBlock); failures closer together count towards giving up.
#define PODCAST_WRITE_PROGRESS (64 * 1024)
#define PODCAST_WRITE_ERRORS_IN_A_ROW 5
#define PODCAST_SLOW_WINDOW_MS 15000
#define PODCAST_SLOW_BYTES (8 * 1024 * 15)

volatile TaskHandle_t podcastTask = nullptr;
volatile bool podcastCancel = false;
// Set when the task starts downloading an episode that was not on the card at all: a new one, which
// then plays first. Otherwise playback continues where it was left (see startPodcastPlayback).
volatile bool podcastNewDownload = false;
bool podcastPreferHttp = true;  // serial "dltls:1" turns it off, to measure TLS during playback

bool podcastBusy() { return podcastTask != nullptr; }

// ---------------------------------------------------------------------------
// SD access of the podcast task: FATFS directly
//
// The main loop and the audio library use the card through the VFS and stdio (File, FILE*,
// file descriptors). With the podcast task in there as well, a track change in the player and a
// reopen in the downloader ended up on one shared descriptor: the player's new file read the
// downloader's write handle (EACCES), the downloader wrote to a descriptor the player had just
// closed (EBADF), and further opens failed. So this task keeps out of the VFS and uses its own
// FATFS objects; FATFS locks the volume itself (FF_FS_REENTRANT). sdGate still keeps the
// player's reads from waiting for the card.
// ---------------------------------------------------------------------------

static char fatDriveLetter = 0;

// "/a/b.mp3" -> "0:/a/b.mp3" on the FATFS drive the SD library mounted.
static String fatPath(const String &path) {
  if (!fatDriveLetter) {
    for (char d = '0'; d <= '1' && !fatDriveLetter; d++) {
      char root[4] = {d, ':', '/', 0};
      FF_DIR dir;
      if (f_opendir(&dir, root) == FR_OK) {
        f_closedir(&dir);
        fatDriveLetter = d;
      }
    }
  }
  return String(fatDriveLetter ? fatDriveLetter : '0') + ":" + path;
}

static bool fatRemove(const String &path) {
  if (!sdGateTake()) return false;
  bool ok = f_unlink(fatPath(path).c_str()) == FR_OK;
  sdGateGive();
  return ok;
}

void logHeap(const char *what) {
  Serial.printf("[heap] %s: free %u, min %u, largest block %u\n", what, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                ESP.getMaxAllocHeap());
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------

// One GET with redirects followed by hand: HTTPClient only follows them within the same
// protocol, and podcast tracking prefixes hop between http and https. HTTP/1.0 keeps servers
// from answering chunked; a server that does it anyway is decoded here.
class HttpStream {
 public:
  int status = 0;
  uint32_t length = SIZE_UNKNOWN;  // Content-Length of the body
  bool hasRange = false;           // a valid Content-Range came back
  uint32_t rangeStart = 0, rangeEnd = 0, rangeTotal = SIZE_UNKNOWN;
  String url;   // where the body comes from, after redirects
  String date;  // the server's Date header
  String error;

  ~HttpStream() { close(); }

  bool open(const String &startUrl, uint32_t from = SIZE_UNKNOWN, uint32_t to = SIZE_UNKNOWN,
            int32_t connectTimeoutMs = 5000) {
    close();
    url = startUrl;
    for (int hop = 0; hop < 10; hop++) {
      bool tls = url.startsWith("https://");
      if (!tls && !url.startsWith("http://")) {
        error = "not an http(s) URL";
        return false;
      }
      WiFiClient *client = &_plain;
      if (tls) {
        if (!_secure) _secure.reset(new WiFiClientSecure());
        _secure->setInsecure();  // see plainHttpVariant() in podcast-feed.h
        _secure->setHandshakeTimeout(15);
        client = _secure.get();
      }
      static const char *keys[] = {"Content-Range", "Transfer-Encoding", "Location", "Date"};
      _http.setReuse(false);
      _http.useHTTP10(true);
      _http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
      _http.setConnectTimeout(connectTimeoutMs);
      _http.setTimeout(10000);
      _http.setUserAgent(PODCAST_USER_AGENT);
      _http.collectHeaders(keys, 4);
      if (!_http.begin(*client, url)) {
        error = "bad URL";
        return false;
      }
      if (from != SIZE_UNKNOWN) {
        _http.addHeader("Range", "bytes=" + String(from) + "-" + (to != SIZE_UNKNOWN ? String(to) : String("")));
      }
      status = _http.GET();
      if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
        String location = _http.header("Location");
        _http.end();
        if (location.isEmpty()) {
          error = "redirect without Location";
          return false;
        }
        url = String(resolveUrl(url.c_str(), location.c_str()).c_str());
        continue;
      }
      break;
    }
    if (status <= 0) {
      error = HTTPClient::errorToString(status);
      _http.end();
      return false;
    }
    if (status != 200 && status != 206) {
      error = "HTTP " + String(status);
      _http.end();
      return false;
    }
    int size = _http.getSize();
    length = size >= 0 ? (uint32_t)size : SIZE_UNKNOWN;
    hasRange = parseContentRange(_http.header("Content-Range").c_str(), rangeStart, rangeEnd, rangeTotal);
    date = _http.header("Date");
    _chunked = _http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
    _left = _chunked ? SIZE_UNKNOWN : length;
    _chunkLeft = 0;
    _chunkEnd = false;
    _stream = _http.getStreamPtr();
    if (!_stream) {
      error = "no stream";
      _http.end();
      return false;
    }
    return true;
  }

  // > 0: bytes read, 0: nothing available right now, -1: the body ended (or the connection did).
  int read(uint8_t *buf, size_t len) {
    if (!_stream) return -1;
    if (_chunked) {
      if (_chunkEnd) return -1;
      if (_chunkLeft == 0 && !readChunkHeader()) return _chunkEnd ? -1 : 0;
      if (len > _chunkLeft) len = _chunkLeft;
    }
    if (_left == 0) return -1;
    if (_left != SIZE_UNKNOWN && len > _left) len = _left;
    int avail = _stream->available();
    if (avail <= 0) return _stream->connected() ? 0 : -1;
    if ((size_t)avail < len) len = avail;
    int n = _stream->read(buf, len);
    if (n <= 0) return 0;
    if (_left != SIZE_UNKNOWN) _left -= n;
    if (_chunked) _chunkLeft -= n;
    return n;
  }

  void close() {
    _stream = nullptr;
    _http.end();
    // end() only stops a client it still believes connected, so one the server has closed
    // already (HTTP/1.0 does after every response) would keep its TLS buffers, ~40 kB, and
    // HTTPClient its pointer to it. Stop both here; the objects live as long as this stream.
    _plain.stop();
    if (_secure) _secure->stop();
  }

 private:
  // Declared before _http, so they outlive it: HTTPClient's destructor still talks to its client.
  WiFiClient _plain;
  std::unique_ptr<WiFiClientSecure> _secure;  // only for https: its constructor allocates 2.2 kB
  HTTPClient _http;
  WiFiClient *_stream = nullptr;
  uint32_t _left = SIZE_UNKNOWN;
  bool _chunked = false;
  uint32_t _chunkLeft = 0;
  bool _chunkEnd = false;

  // "<hex>[;extension]\r\n" after the CRLF that ends the previous chunk. Rare, so it simply waits.
  bool readChunkHeader() {
    char line[24];
    size_t used = 0;
    unsigned long start = millis();
    while (millis() - start < 10000) {
      int c = _stream->read();
      if (c < 0) {
        if (!_stream->connected()) break;
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }
      if (c == '\n') {
        line[used] = 0;
        if (used == 0 || (used == 1 && line[0] == '\r')) continue;  // CRLF after chunk data
        _chunkLeft = strtoul(line, nullptr, 16);
        if (_chunkLeft == 0) break;  // last chunk
        return true;
      }
      if (used < sizeof(line) - 1) line[used++] = (char)c;
    }
    _chunkEnd = true;
    return false;
  }
};

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------

bool podcastWifiUp(String &error) {
  String ssid, pass;
  if (!readSavedCredentials(ssid, pass)) {
    error = "no saved Wi-Fi network";
    return false;
  }
  WiFi.persistent(false);  // never write the credentials to flash: that stalls the audio
  WiFi.setAutoReconnect(false);
  WiFi.setHostname(boxHostname());
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep halves the download rate
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : nullptr);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (podcastCancel || millis() - start > PODCAST_WIFI_TIMEOUT_MS) {
      error = podcastCancel ? "cancelled" : "could not join \"" + ssid + "\"";
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  Serial.printf("[podcast] joined \"%s\" after %lu ms, rssi %d\n", ssid.c_str(), millis() - start, WiFi.RSSI());
  return true;
}

void podcastWifiDown() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// ---------------------------------------------------------------------------
// Episode download
// ---------------------------------------------------------------------------

struct DownloadStats {
  uint32_t bytes = 0;  // received in this session
  unsigned long startedMs = 0;
  uint32_t maxWriteMs = 0, maxFlushMs = 0;
  uint32_t writeRetries = 0;  // failed writes repeated in place
  uint32_t minFree = 0xFFFFFFFF, minBlock = 0xFFFFFFFF;
  unsigned long lastReport = 0;

  void sample() {
    uint32_t f = ESP.getFreeHeap(), b = ESP.getMaxAllocHeap();
    if (f < minFree) minFree = f;
    if (b < minBlock) minBlock = b;
  }
};

// What is already on the card for `path`.
static void localEpisodeState(const String &path, uint32_t &size, bool &startsWithId3) {
  size = 0;
  startsWithId3 = false;
  FIL *f = new (std::nothrow) FIL;
  if (!f) return;
  if (sdGateTake()) {
    if (f_open(f, fatPath(path).c_str(), FA_READ) == FR_OK) {
      size = f_size(f);
      uint8_t head[3];
      UINT got = 0;
      startsWithId3 = size >= 3 && f_read(f, head, 3, &got) == FR_OK && got == 3 && memcmp(head, "ID3", 3) == 0;
      f_close(f);
    }
    sdGateGive();
  }
  delete f;
}

// Reads exactly `len` bytes unless the body ends or stalls. Returns the count.
static size_t readFully(HttpStream &http, uint8_t *buf, size_t len) {
  size_t got = 0;
  unsigned long last = millis();
  while (got < len && !podcastCancel) {
    int n = http.read(buf + got, len - got);
    if (n < 0) break;
    if (n == 0) {
      if (millis() - last > PODCAST_STALL_TIMEOUT_MS) break;
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    got += n;
    last = millis();
  }
  return got;
}

// Downloads `url` into `path`, leaving out a leading ID3 tag (podcast covers reach 1 MB, and
// the audio library skips a tag only 256 bytes per loop) and continuing a partial file with a
// Range request. Progress is published through `growing`, so the file plays while it grows.
// One attempt; returns true when the file is complete.
// Called once the server has been asked: what will be downloaded, before the first byte.
typedef std::function<void(uint32_t serverTotal, const ResumePlan &plan)> DownloadPlanned;

// What the first attempt of a session found out, so that a retry goes straight to the file:
// the probe follows the redirect chain, often over https, and while an episode plays there is
// no room for a TLS handshake.
struct EpisodeSource {
  bool known = false;
  String url;  // where the body came from (plain http if the host serves it)
  uint32_t skip = 0, total = SIZE_UNKNOWN;
};

static bool downloadEpisodeOnce(const String &url, const String &path, bool allowSkip, uint32_t savedTotal,
                                String &error, DownloadStats &st, bool &retrySkipless, bool &writeError,
                                bool &tooSlow, uint32_t &serverTotal, const DownloadPlanned &planned,
                                EpisodeSource &src) {
  retrySkipless = false;
  writeError = false;
  tooSlow = false;
  serverTotal = SIZE_UNKNOWN;
  uint32_t local;
  bool localHasId3;
  localEpisodeState(path, local, localHasId3);

  HttpStream probe;
  uint8_t hdr[10];
  size_t hdrLen = 0;
  bool ranges;
  uint32_t total, tagSize;
  if (src.known) {  // a retry: the server already told us everything
    ranges = true;
    total = src.total;
    tagSize = src.skip;
  } else {
    // Probe: the first 10 bytes tell the ID3 size, Content-Range the total and range support.
    // Plain http first: it saves a TLS handshake (~3 s) before the first sound, and most hosts
    // serve the file both ways. Only a real range answer counts, never some 200 page.
    String plain = String(plainHttpVariant(url.c_str()).c_str());
    bool probed = podcastPreferHttp && plain != url && probe.open(plain, 0, 9, 3000) && probe.status == 206 &&
                  probe.hasRange;
    if (!probed && !probe.open(url, 0, 9)) {
      error = "episode: " + probe.error;
      return false;
    }
    hdrLen = readFully(probe, hdr, sizeof(hdr));
    ranges = probe.status == 206 && probe.hasRange;
    total = ranges ? probe.rangeTotal : probe.length;
    tagSize = id3TagSize(hdr, hdrLen);
  }
  serverTotal = total;
  ResumePlan plan = planResume(local, localHasId3, ranges, total, tagSize, allowSkip, savedTotal);
  uint32_t skip = plan.skip, expected = plan.expected;
  Serial.printf("[podcast] %s: %s, total %d, id3 %u, on card %u%s\n", path.c_str(), ranges ? "ranges ok" : "no ranges",
                total == SIZE_UNKNOWN ? -1 : (int)total, skip, local,
                plan.complete ? ", complete" : plan.restart ? ", starting over" : "");
  if (plan.complete) {
    growingDone(local);
    return true;
  }
  if (plan.restart) local = 0;
  if (planned) planned(total, plan);

  HttpStream body;
  HttpStream *in = &probe;
  uint32_t discard = 0;  // bytes at the start of `in` that do not go into the file
  uint8_t *pending = nullptr;
  size_t pendingLen = 0;
  if (ranges) {
    probe.close();
    uint32_t from = plan.from;
    auto sameFile = [&]() { return body.status == 206 && body.hasRange && body.rangeStart == from && body.rangeTotal == total; };
    bool ok;
    if (src.known) {
      ok = body.open(src.url, from) && sameFile();
      if (!ok) src.known = false;  // e.g. a signed URL expired: the next attempt probes again
    } else {
      // Prefer plain HTTP for the long transfer, but only if it serves the same file.
      String plain = String(plainHttpVariant(probe.url.c_str()).c_str());
      ok = podcastPreferHttp && plain != probe.url && body.open(plain, from, SIZE_UNKNOWN, 3000) && sameFile();
      if (!ok) ok = body.open(probe.url, from) && sameFile();
      if (ok) {
        src.known = true;
        src.url = body.url;
        src.skip = plan.skip;
        src.total = total;
      }
    }
    if (!ok) {
      error = "episode: " + (body.error.length() ? body.error : String("range request refused"));
      return false;
    }
    Serial.printf("[podcast] body from %s at %u\n", body.url.startsWith("https") ? "https" : "http", from);
    in = &body;
  } else {
    // The probe connection carries the whole file; its first bytes are already in hdr.
    discard = skip;
    if (hdrLen > discard) {
      pending = hdr + discard;
      pendingLen = hdrLen - discard;
      discard = 0;
    } else {
      discard -= hdrLen;
    }
  }

  uint8_t *block = (uint8_t *)malloc(PODCAST_BLOCK);
  if (!block) {
    error = "no memory for the download buffer";
    return false;
  }
  FIL *out = new (std::nothrow) FIL;
  bool opened = false;
  if (out && sdGateTake()) {
    // In one gated section with the open, which truncates on a restart: see gated-fs.h.
    growingStart(path.c_str(), local, expected == SIZE_UNKNOWN ? 0 : expected);
    opened = f_open(out, fatPath(path).c_str(), FA_WRITE | (local > 0 ? FA_OPEN_APPEND : FA_CREATE_ALWAYS)) == FR_OK;
    sdGateGive();
  }
  if (!opened) {
    delete out;
    free(block);
    error = "cannot write " + path;
    return false;
  }

  uint32_t written = local, flushed = local;
  size_t fill = 0;
  bool checkedSync = local > 0 || skip == 0;
  bool ended = false, stalled = false;
  unsigned long lastData = millis();
  if (pendingLen) {
    memcpy(block, pending, pendingLen);
    fill = pendingLen;
  }

  // After a failed write FATFS refuses the handle for good. Closing it saves what was accepted;
  // the reopened file has to end exactly at `at`, or the caller starts over from the card.
  auto reopenAt = [&](uint32_t at) -> bool {
    if (!sdGateTake()) return false;
    f_close(out);
    bool ok = f_open(out, fatPath(path).c_str(), FA_WRITE | FA_OPEN_APPEND) == FR_OK;
    if (ok && f_size(out) != at) {
      Serial.printf("[podcast] reopened %s ends at %u, not %u\n", path.c_str(), (unsigned)f_size(out), (unsigned)at);
      f_close(out);
      ok = false;
    }
    sdGateGive();
    return ok;
  };
  // The SD driver fails a write now and then although the card has taken it: after each
  // multi-sector write it asks for the card status, and any bit set there counts as an error.
  // That happens every few hundred kB on some cards; repeating the rest of the block in place
  // costs a few ms, where a new connection costs half a second (or a TLS handshake).
  auto writeBlock = [&](size_t len) -> bool {
    size_t done = 0;
    for (int tries = 0;; tries++) {
      unsigned long t0 = millis();
      if (!sdGateTake()) return false;
      UINT put = 0;
      FRESULT res = f_write(out, block + done, len - done, &put);
      sdGateGive();
      uint32_t ms = millis() - t0;
      if (ms > st.maxWriteMs) st.maxWriteMs = ms;
      done += put;
      if (res == FR_OK && done == len) break;
      if (tries < 2 && st.writeRetries < 10) {
        Serial.printf("[podcast] write of %u bytes at %u failed after %u ms (FATFS error %d), writing it again\n",
                      (unsigned)(len - done + put), (unsigned)(written + done - put), ms, (int)res);
      }
      if (tries >= 2 || !reopenAt(written + done)) {
        Serial.printf("[podcast] write of %u bytes at %u put %u after %u ms, FATFS error %d, heap %u, largest %u\n",
                      (unsigned)len, written, (unsigned)done, ms, (int)res, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        return false;  // the caller starts over from what the card has
      }
      st.writeRetries++;
    }
    written += len;
    return true;
  };
  auto flushOut = [&]() {
    unsigned long t0 = millis();
    if (!sdGateTake()) return;
    f_sync(out);  // updates the directory entry: readers that open the file now see this size
    sdGateGive();
    uint32_t ms = millis() - t0;
    if (ms > st.maxFlushMs) st.maxFlushMs = ms;
    flushed = written;
    growingSynced(flushed);
  };

  bool writeFailed = false;
  unsigned long rateSince = millis();
  uint32_t rateBytes = 0;
  while (!podcastCancel) {
    if (millis() - rateSince >= PODCAST_SLOW_WINDOW_MS) {
      if (rateBytes < PODCAST_SLOW_BYTES) {
        tooSlow = true;
        break;
      }
      rateSince = millis();
      rateBytes = 0;
    }
    int n = in->read(block + fill, PODCAST_BLOCK - fill);
    if (n < 0) {
      ended = true;
      break;
    }
    if (n == 0) {
      if (millis() - lastData > PODCAST_STALL_TIMEOUT_MS) {
        stalled = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    lastData = millis();
    st.bytes += n;
    rateBytes += n;
    if (discard) {  // still inside the ID3 tag of a server without ranges
      size_t d = (size_t)n < discard ? n : discard;
      memmove(block + fill, block + fill + d, n - d);
      n -= d;
      discard -= d;
    }
    fill += n;
    if (!checkedSync && fill >= 2) {
      checkedSync = true;
      if (!isMp3FrameSync(block[0], block[1])) {  // something else follows the tag: keep it all
        retrySkipless = true;
        error = "no MP3 frame after the ID3 tag";
        break;
      }
    }
    if (fill == PODCAST_BLOCK) {
      if (!writeBlock(fill)) {
        writeFailed = true;
        break;
      }
      fill = 0;
      uint32_t since = written - flushed;
      if ((flushed == local && written - local >= PODCAST_FIRST_FLUSH) || since >= PODCAST_FLUSH_EVERY ||
          (since >= PODCAST_HUNGRY_FLUSH && growingHungry())) {
        flushOut();
      }
    }
    if (millis() - st.lastReport >= 2000) {
      st.lastReport = millis();
      st.sample();
      unsigned long secs = (millis() - st.startedMs) / 1000;
      Serial.printf("[podcast] %u kB on card, %lu kB/s, heap %u (min %u), largest %u, write max %u ms, flush max %u ms\n",
                    written / 1024, secs ? st.bytes / 1024 / secs : 0, ESP.getFreeHeap(), st.minFree,
                    ESP.getMaxAllocHeap(), st.maxWriteMs, st.maxFlushMs);
    }
  }
  if (fill && !retrySkipless && !writeFailed && !tooSlow) writeBlock(fill);
  if (sdGateTake()) {
    f_close(out);  // after a failed write the file keeps what was written before it
    sdGateGive();
  }
  delete out;
  free(block);

  if (retrySkipless) {
    fatRemove(path);
    growingDone(0);
    return false;
  }
  bool complete = expected != SIZE_UNKNOWN ? written == expected : ended && !stalled && !writeFailed;
  if (complete) {
    growingDone(written);
    return true;
  }
  growingSynced(written);
  writeError = writeFailed;
  if (writeFailed) error = "writing to the SD card failed";
  else if (tooSlow) error = "the connection got too slow";
  else if (podcastCancel) error = "cancelled";
  else if (stalled) error = "download stalled";
  else error = "connection closed early";
  return false;
}

// With retries: a dropped connection continues where it stopped. savedTotal: the server size the
// file on the card was started with (SIZE_UNKNOWN if not known); serverTotal: the size now.
//
// A failed write continues too, right away and against a limit of its own: the core's SD driver
// reports a write as failed when the card's status has any bit set afterwards, also a stale one
// left by an earlier read, and that happens every few MB while an episode plays. The file is then
// closed, it keeps what was flushed, and the next round asks the server from there.
bool downloadEpisode(const String &url, const String &path, uint32_t savedTotal, String &error, uint32_t &serverTotal,
                     const DownloadPlanned &planned = nullptr) {
  static const uint16_t backoffMs[] = {2000, 5000, 10000};
  DownloadStats st;
  st.startedMs = millis();
  serverTotal = SIZE_UNKNOWN;
  EpisodeSource src;
  bool allowSkip = true;
  bool ok = false;
  int writeErrors = 0, writeErrorsInARow = 0, slowConnections = 0;
  uint32_t bytesAtWriteError = 0;
  for (int attempt = 0; attempt <= 3 && !podcastCancel; attempt++) {
    bool retrySkipless = false, writeError = false, tooSlow = false;
    ok = downloadEpisodeOnce(url, path, allowSkip, savedTotal, error, st, retrySkipless, writeError, tooSlow,
                             serverTotal, planned, src);
    if (serverTotal != SIZE_UNKNOWN) savedTotal = serverTotal;  // what is on the card now matches it
    if (ok) break;
    if (retrySkipless) {
      allowSkip = false;
      src.known = false;
      attempt--;  // not a network failure
      continue;
    }
    if (writeError) {
      // Only failures without progress in between mean that the card is in trouble.
      bool progress = st.bytes - bytesAtWriteError >= PODCAST_WRITE_PROGRESS;
      writeErrorsInARow = progress ? 1 : writeErrorsInARow + 1;
      bytesAtWriteError = st.bytes;
      writeErrors++;
      if (writeErrorsInARow <= PODCAST_WRITE_ERRORS_IN_A_ROW) {
        attempt--;  // the network is fine: continue at once
        continue;
      }
    }
    if (tooSlow && ++slowConnections <= 20) {
      Serial.printf("[podcast] connection too slow, reconnecting (%d)\n", slowConnections);
      attempt--;
      continue;
    }
    Serial.printf("[podcast] attempt %d: %s\n", attempt + 1, error.c_str());
    if (attempt < 3 && !podcastCancel) vTaskDelay(pdMS_TO_TICKS(backoffMs[attempt]));
  }
  if (st.writeRetries || writeErrors) {
    Serial.printf("[podcast] failed writes: %u written again in place, %d continued over a new connection\n",
                  (unsigned)st.writeRetries, writeErrors);
  }
  st.sample();
  unsigned long secs = (millis() - st.startedMs) / 1000;
  Serial.printf("[podcast] %s %s: %u kB in %lu s, min heap %u, min largest %u, write max %u ms, flush max %u ms\n",
                path.c_str(), ok ? "complete" : "incomplete", st.bytes / 1024, secs, st.minFree, st.minBlock,
                st.maxWriteMs, st.maxFlushMs);
  if (!ok) {
    uint32_t synced, expected;
    bool done;
    if (growingGet(path.c_str(), synced, expected, done)) growingDone(synced);  // readers end cleanly
  }
  return ok;
}

// ---------------------------------------------------------------------------
// Album files: podcast.txt (written by the web UI) and podcast_state.txt (by the box)
// ---------------------------------------------------------------------------

#define PODCAST_CONFIG_FILE "/podcast.txt"
#define PODCAST_STATE_FILE "/podcast_state.txt"
#define PODCAST_STATE_TMP "/podcast_state.tmp"
#define PODCAST_KEEP_DEFAULT 30
#define PODCAST_KEEP_MAX 500

struct PodcastConfig {
  String url;
  int keep = PODCAST_KEEP_DEFAULT;  // episodes kept in the folder
};

// What the box last found out about the feed. Only the newest episode is tracked.
struct PodcastState {
  String file;  // file name of the newest episode
  String guid, title, date;
  uint32_t total = SIZE_UNKNOWN;  // server size the file was started with
  uint32_t skip = 0;              // ID3 bytes left out
  uint32_t size = 0;              // expected size on the card
  bool complete = false;
  String result;   // ok, offline, error, unsupported
  String error;    // why, if not ok
  String checked;  // the feed server's Date of the last check
  String feedTitle, newFeedUrl;
};

static String readSmallFile(const String &path) {
  String content;
  FIL *f = new (std::nothrow) FIL;
  if (!f) return content;
  if (sdGateTake()) {
    if (f_open(f, fatPath(path).c_str(), FA_READ) == FR_OK) {
      char buf[128];
      UINT got = 0;
      while (content.length() < 2048 && f_read(f, buf, sizeof(buf), &got) == FR_OK && got > 0) {
        for (UINT i = 0; i < got; i++) content += buf[i];
      }
      f_close(f);
    }
    sdGateGive();
  }
  delete f;
  return content;
}

// Line by line, "key=value"; calls back for each pair (the first podcast.txt line is the URL).
template <typename F>
static void forEachLine(const String &content, F f) {
  int start = 0;
  while (start < (int)content.length()) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = content.length();
    String line = content.substring(start, end);
    line.trim();
    if (line.length()) f(line);
    start = end + 1;
  }
}

bool readPodcastConfig(const String &dir, PodcastConfig &cfg) {
  cfg = PodcastConfig();
  bool first = true;
  forEachLine(readSmallFile(dir + PODCAST_CONFIG_FILE), [&](const String &line) {
    if (first && !line.startsWith("keep=")) cfg.url = line;
    else if (line.startsWith("keep=")) cfg.keep = constrain(line.substring(5).toInt(), 1, PODCAST_KEEP_MAX);
    first = false;
  });
  return cfg.url.startsWith("http://") || cfg.url.startsWith("https://");
}

static bool writeSmallFile(const String &path, const String &content) {
  FIL *f = new (std::nothrow) FIL;
  if (!f) return false;
  bool ok = false;
  if (sdGateTake()) {
    if (f_open(f, fatPath(path).c_str(), FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
      UINT put = 0;
      ok = f_write(f, content.c_str(), content.length(), &put) == FR_OK && put == content.length();
      ok = f_close(f) == FR_OK && ok;
    }
    sdGateGive();
  }
  delete f;
  return ok;
}

bool writePodcastConfig(const String &dir, const PodcastConfig &cfg) {
  return writeSmallFile(dir + PODCAST_CONFIG_FILE, cfg.url + "\nkeep=" + String(cfg.keep) + "\n");
}

bool readPodcastState(const String &dir, PodcastState &st) {
  st = PodcastState();
  bool any = false;
  forEachLine(readSmallFile(dir + PODCAST_STATE_FILE), [&](const String &line) {
    int eq = line.indexOf('=');
    if (eq < 0) return;
    String k = line.substring(0, eq), v = line.substring(eq + 1);
    any = true;
    if (k == "file") st.file = v;
    else if (k == "guid") st.guid = v;
    else if (k == "title") st.title = v;
    else if (k == "date") st.date = v;
    else if (k == "total") st.total = v == "?" ? SIZE_UNKNOWN : strtoul(v.c_str(), nullptr, 10);
    else if (k == "skip") st.skip = strtoul(v.c_str(), nullptr, 10);
    else if (k == "size") st.size = strtoul(v.c_str(), nullptr, 10);
    else if (k == "complete") st.complete = v == "1";
    else if (k == "result") st.result = v;
    else if (k == "error") st.error = v;
    else if (k == "checked") st.checked = v;
    else if (k == "feed") st.feedTitle = v;
    else if (k == "newfeed") st.newFeedUrl = v;
  });
  return any;
}

static String oneLine(String v) {
  v.replace('\n', ' ');
  v.replace('\r', ' ');
  return v;
}

// Through a temp file, so a power cut leaves the old or the new state, never half of one.
bool writePodcastState(const String &dir, const PodcastState &st) {
  String c = "file=" + st.file + "\nguid=" + oneLine(st.guid) + "\ntitle=" + oneLine(st.title) + "\ndate=" + st.date +
             "\ntotal=" + (st.total == SIZE_UNKNOWN ? String("?") : String(st.total)) + "\nskip=" + String(st.skip) +
             "\nsize=" + String(st.size) + "\ncomplete=" + (st.complete ? "1" : "0") + "\nresult=" + st.result +
             "\nerror=" + oneLine(st.error) + "\nchecked=" + oneLine(st.checked) + "\nfeed=" + oneLine(st.feedTitle) +
             "\nnewfeed=" + oneLine(st.newFeedUrl) + "\n";
  if (!writeSmallFile(dir + PODCAST_STATE_TMP, c)) return false;
  if (!sdGateTake()) return false;
  f_unlink(fatPath(dir + PODCAST_STATE_FILE).c_str());
  bool ok = f_rename(fatPath(dir + PODCAST_STATE_TMP).c_str(), fatPath(dir + PODCAST_STATE_FILE).c_str()) == FR_OK;
  sdGateGive();
  return ok;
}

// The episode files in `dir`, oldest first.
std::vector<String> listEpisodes(const String &dir) {
  std::vector<String> names;
  FF_DIR *d = new (std::nothrow) FF_DIR;
  FILINFO *fi = new (std::nothrow) FILINFO;
  if (d && fi && sdGateTake()) {
    if (f_opendir(d, fatPath(dir).c_str()) == FR_OK) {
      while (f_readdir(d, fi) == FR_OK && fi->fname[0]) {
        if (!(fi->fattrib & AM_DIR) && isEpisodeFileName(fi->fname)) names.push_back(fi->fname);
      }
      f_closedir(d);
    }
    sdGateGive();
  }
  delete d;
  delete fi;
  std::sort(names.begin(), names.end(), [](const String &a, const String &b) { return a.compareTo(b) < 0; });
  return names;
}

static void removeIndexFile(const String &dir) {
  fatRemove(dir + "/index.txt");  // the player rebuilds its playlist from the folder
}

static uint32_t fileSizeOnCard(const String &path) {
  uint32_t size = 0;
  FILINFO *fi = new (std::nothrow) FILINFO;
  if (fi && sdGateTake()) {
    if (f_stat(fatPath(path).c_str(), fi) == FR_OK) size = fi->fsize;
    sdGateGive();
  }
  delete fi;
  return size;
}

// Deletes the oldest episodes beyond `keep`, never `except`. Only runs while nothing plays.
int podcastRetention(const String &dir, int keep, const String &except) {
  std::vector<String> names = listEpisodes(dir);
  int removed = 0;
  for (size_t i = 0; i + keep < names.size(); i++) {
    if (names[i] == except) continue;
    if (fatRemove(dir + "/" + names[i])) removed++;
    Serial.printf("[podcast] removed old episode %s\n", names[i].c_str());
  }
  return removed;
}

// ---------------------------------------------------------------------------
// The podcast task
// ---------------------------------------------------------------------------

enum PodcastPhase : uint8_t {
  POD_IDLE,
  POD_CHECKING,     // joining Wi-Fi, reading the feed
  POD_DOWNLOADING,  // the newest episode is coming in (podcastPlayable() says when it can start)
  POD_UP_TO_DATE,   // the newest episode was already complete on the card
  POD_COMPLETE,     // downloaded it now
  POD_OFFLINE,      // no network: play what is on the card
  POD_FAILED,
  POD_UNSUPPORTED  // the newest episode is not an MP3
};

static const char *podcastPhaseName(PodcastPhase p) {
  static const char *names[] = {"idle", "checking", "downloading", "up to date", "complete", "offline", "failed", "unsupported"};
  return p <= POD_UNSUPPORTED ? names[p] : "?";
}

volatile PodcastPhase podcastPhase = POD_IDLE;
String podcastTaskDir;               // album folder of the running task; set before it starts
char podcastEpisodePath[260] = "";  // newest episode, under growingMux

String podcastEpisode() {
  char copy[sizeof(podcastEpisodePath)];
  portENTER_CRITICAL(&growingMux);
  strlcpy(copy, podcastEpisodePath, sizeof(copy));
  portEXIT_CRITICAL(&growingMux);
  return String(copy);
}

static void setPodcastEpisode(const String &path) {
  portENTER_CRITICAL(&growingMux);
  strlcpy(podcastEpisodePath, path.c_str(), sizeof(podcastEpisodePath));
  portEXIT_CRITICAL(&growingMux);
}

// The newest episode can start: complete, or enough of it on the card.
bool podcastPlayable() {
  PodcastPhase p = podcastPhase;
  if (p == POD_UP_TO_DATE || p == POD_COMPLETE) return true;
  if (p != POD_DOWNLOADING) return false;
  uint32_t synced, expected;
  bool done;
  String path = podcastEpisode();
  return path.length() && growingGet(path.c_str(), synced, expected, done) && (done || synced >= PODCAST_FIRST_FLUSH);
}

static bool fetchFeed(const String &url, FeedParser &parser, String &error, String &serverDate) {
  HttpStream http;
  if (!http.open(url)) {
    error = "feed: " + http.error;
    return false;
  }
  serverDate = http.date;
  char buf[1024];
  unsigned long last = millis();
  while (!parser.done() && !podcastCancel) {
    int n = http.read((uint8_t *)buf, sizeof(buf));
    if (n < 0) break;
    if (n == 0) {
      if (millis() - last > PODCAST_STALL_TIMEOUT_MS) break;
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    last = millis();
    parser.push(buf, n);
  }
  http.close();  // frees the TLS session before the long transfer
  Serial.printf("[podcast] feed: %u bytes read, %d items, \"%s\"\n", parser.bytes, parser.itemsSeen,
                parser.channelTitle.c_str());
  if (!parser.newest().valid()) {
    error = parser.itemsSeen ? "the feed has no audio episodes" : "not a podcast feed (RSS)";
    return false;
  }
  return true;
}

// The whole check and download; returns how it ended. A function of its own, so its Strings are
// destroyed before the task deletes itself (vTaskDelete does not return, nothing would free them).
static PodcastPhase podcastRun() {
  String dir = podcastTaskDir;
  PodcastConfig cfg;
  PodcastState st;
  readPodcastState(dir, st);
  if (!readPodcastConfig(dir, cfg)) {
    st.result = "error";
    st.error = "no feed URL in podcast.txt";
    writePodcastState(dir, st);
    return POD_FAILED;
  }
  if (podcastRetention(dir, cfg.keep, st.file) > 0) removeIndexFile(dir);

  String error;
  if (!podcastWifiUp(error)) {
    Serial.printf("[podcast] offline: %s\n", error.c_str());
    st.result = "offline";
    st.error = error;
    writePodcastState(dir, st);
    return POD_OFFLINE;
  }

  FeedParser parser;
  String serverDate;
  if (!fetchFeed(cfg.url, parser, error, serverDate)) {
    Serial.printf("[podcast] %s\n", error.c_str());
    st.result = "error";
    st.error = error;
    writePodcastState(dir, st);
    return POD_FAILED;
  }
  const PodcastEpisode &ep = parser.newest();
  String file = String(episodeFileName(ep).c_str());
  String path = dir + "/" + file;
  bool same = st.file == file;
  if (!same) {  // a new episode: forget the old one's progress
    PodcastState fresh;
    st = fresh;
    st.file = file;
  }
  st.guid = String(ep.guid.c_str());
  st.title = String(ep.title.c_str());
  char date[20];
  snprintf(date, sizeof(date), "%04d-%02d-%02d %02d:%02d", ep.date.year, ep.date.month, ep.date.day, ep.date.hour,
           ep.date.minute);
  st.date = ep.dated ? String(date) : String("");
  st.checked = serverDate;
  st.feedTitle = String(parser.channelTitle.c_str());
  st.newFeedUrl = String(parser.newFeedUrl.c_str());
  Serial.printf("[podcast] newest: %s (%s), %s\n", st.title.c_str(), st.date.c_str(), ep.type.c_str());

  if (!ep.isMp3()) {
    st.result = "unsupported";
    st.error = "the newest episode is " + String(ep.type.length() ? ep.type.c_str() : "not an MP3") + ", not supported yet";
    writePodcastState(dir, st);
    return POD_UNSUPPORTED;
  }

  uint32_t onCard = fileSizeOnCard(path);
  if (same && st.complete && onCard > 0 && onCard == st.size) {
    st.result = "ok";
    st.error = "";
    writePodcastState(dir, st);
    setPodcastEpisode(path);
    Serial.printf("[podcast] up to date: %s\n", path.c_str());
    return POD_UP_TO_DATE;
  }

  st.complete = false;
  st.result = "downloading";
  writePodcastState(dir, st);
  if (onCard == 0) removeIndexFile(dir);  // a new file appears: the playlist has to be rebuilt
  setPodcastEpisode(path);
  podcastNewDownload = onCard == 0;
  podcastPhase = POD_DOWNLOADING;
  uint32_t serverTotal;
  bool ok = downloadEpisode(String(ep.url.c_str()), path, same ? st.total : SIZE_UNKNOWN, error, serverTotal,
                            [&](uint32_t total, const ResumePlan &plan) {
                              // Saved before the first byte: after a power cut the next try knows
                              // which server file the partial one belongs to.
                              st.total = total;
                              st.skip = plan.skip;
                              st.size = plan.expected == SIZE_UNKNOWN ? 0 : plan.expected;
                              writePodcastState(dir, st);
                            });
  st.complete = ok;
  if (ok) st.size = fileSizeOnCard(path);
  st.result = ok ? "ok" : "error";
  st.error = ok ? "" : error;
  writePodcastState(dir, st);
  return ok ? POD_COMPLETE : POD_FAILED;
}

static void podcastTaskMain(void *) {
  PodcastPhase phase = podcastRun();
  podcastWifiDown();
  logHeap("podcast task end");
  Serial.printf("[podcast] %s\n", podcastPhaseName(phase));
  podcastPhase = phase;
  podcastTask = nullptr;
  vTaskDelete(NULL);
}

void podcastStop(bool wait, unsigned long timeoutMs = 3000);

// Starts checking the feed of the podcast album `dir`. A task already working on the same album
// is kept (the token was put back); one for another album is stopped first.
bool podcastStart(const String &dir) {
  if (podcastTask) {
    if (podcastTaskDir == dir) return true;
    podcastStop(true);
  }
  podcastCancel = false;
  podcastNewDownload = false;
  podcastPhase = POD_CHECKING;
  podcastTaskDir = dir;
  setPodcastEpisode("");
  growingClear();
  TaskHandle_t handle = nullptr;
  logHeap("podcast task start");
  if (xTaskCreatePinnedToCore(podcastTaskMain, "podcast", PODCAST_TASK_STACK, nullptr, 1, &handle, 0) != pdPASS) {
    podcastPhase = POD_FAILED;
    return false;
  }
  podcastTask = handle;
  return true;
}

// Stops the task. With `wait`, returns once it is gone (the episode file closed) or after
// `timeoutMs`; a TLS handshake in progress can take up to 15 s to give up.
void podcastStop(bool wait, unsigned long timeoutMs) {
  if (!podcastTask) return;
  podcastCancel = true;
  unsigned long start = millis();
  while (wait && podcastTask && millis() - start < timeoutMs) {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------------
// Serial test commands (dl:<url>, md5:<path>)
// ---------------------------------------------------------------------------

struct PodcastDebugJob {
  String url;
  String path;
};

static void podcastDebugTask(void *arg) {
  {  // a scope of its own: everything in it is freed before the task deletes itself
    std::unique_ptr<PodcastDebugJob> job((PodcastDebugJob *)arg);
    logHeap("download task start");
    String error;
    if (podcastWifiUp(error)) {
      logHeap("wifi up");
      uint32_t total;
      if (!downloadEpisode(job->url, job->path, SIZE_UNKNOWN, error, total)) Serial.printf("[podcast] failed: %s\n", error.c_str());
      podcastWifiDown();
      logHeap("wifi down");
    } else {
      Serial.printf("[podcast] no network: %s\n", error.c_str());
    }
  }
  podcastTask = nullptr;
  vTaskDelete(NULL);
}

bool podcastDebugDownload(const String &url, const String &path);
String podcastDebugPath;  // where the last dl: went

// dl:<url> -> /spike_<hash>.mp3: one file per URL, so a test never appends one episode to another.
bool podcastDebugDownload(const String &url) {
  char path[24];
  snprintf(path, sizeof(path), "/spike_%04x.mp3", (unsigned)(episodeIdHash(url.c_str()) & 0xFFFF));
  podcastDebugPath = path;
  return podcastDebugDownload(url, podcastDebugPath);
}

bool podcastDebugDownload(const String &url, const String &path) {
  if (podcastTask) return false;
  podcastCancel = false;
  PodcastDebugJob *job = new PodcastDebugJob{url, path};
  TaskHandle_t handle = nullptr;
  if (xTaskCreatePinnedToCore(podcastDebugTask, "podcast", PODCAST_TASK_STACK, job, 1, &handle, 0) != pdPASS) {
    delete job;
    return false;
  }
  podcastTask = handle;
  return true;
}

// MD5 of a file on the card, to compare a download with `curl ... | md5sum`.
String fileMd5(const String &path, uint32_t &size) {
  size = 0;
  MD5Builder md5;
  md5.begin();
  uint8_t *buf = (uint8_t *)malloc(PODCAST_BLOCK);
  if (!buf) return "no memory";
  if (!sdGateTake()) {
    free(buf);
    return "card busy";
  }
  File f = SD.open(path, FILE_READ);
  if (!f) {
    sdGateGive();
    free(buf);
    return "no such file";
  }
  int n;
  while ((n = f.read(buf, PODCAST_BLOCK)) > 0) {
    md5.add(buf, n);
    size += n;
    esp_task_wdt_reset();
  }
  f.close();
  sdGateGive();
  free(buf);
  md5.calculate();
  return md5.toString();
}

#endif
