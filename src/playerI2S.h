#ifndef PLAYERI2S_CLASS
#define PLAYERI2S_CLASS

#include <player.cpp>

#include "Audio.h"
#include "gated-fs.h"
#include "management-server.h"
#include "podcast.h"

#define I2S_DOUT 25
#define I2S_BCLK GPIO_NUM_27  // I2S
#define I2S_LRC GPIO_NUM_26

class PlayerI2S : public Player {
 private:
  // While a podcast download runs, every file goes through gatedFS, so the decoder never waits
  // for the card the downloader writes to.
  fs::FS &card() { return podcastBusy() ? gatedFS : (fs::FS &)SD; }

  // connecttoFS() also fails when the decoder cannot get its ~23 kB of buffers. Only a card that
  // no longer answers is worth the reboot in loop().
  bool startFile(const char *path, uint32_t pos) {
    if (audio.connecttoFS(card(), path, pos)) return true;
    Serial.printf("could not start %s, free heap %u, largest block %u\n", path, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    File root = SD.open("/");
    sdConnected = (bool)root;
    if (root) root.close();
    return false;
  }

 public:
  Audio audio = Audio();

  PlayerI2S() {}

  void startAudio() {
    // The library installs I2S with 16 DMA buffers of 512 frames: 32 kB, ~186 ms of sound. Half
    // of that is plenty for this loop, and the 16 kB it frees is what a TLS download (podcasts
    // served only over https) needs next to the decoder. Same settings as the library otherwise.
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = 16000;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 512;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = I2S_PIN_NO_CHANGE;
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_zero_dma_buffer(I2S_NUM_0);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.forceMono(true);
  }

  // Frees the audio engine and playlist (RAM for WiFi) and starts the web server
  // with the Wi-Fi setup page. Files/Upload appear once an album is selected.
  void startManagementServer() {
    audio.~Audio();
    playlist.~LList();
    setupWebServer();
  }

  // Resolves the album folder for the token and publishes it to the web UI.
  // Both touch the SD card, so this runs under the web server's file lock.
  void selectAlbumForManagement(String tag) {
    if (!filesLock(10000)) {
      Serial.println("selectAlbumForManagement: file lock busy");
      return;
    }
    determineAlbumDir(tag);
    setAlbumFolder(this->albumDir, tag);
    filesUnlock();
  }

  // Starts `path` at byte `pos` (0 = from the start).
  bool playTrack(String path, uint32_t pos = 0) override {
    if (!card().exists(path)) {
      // The index lists a track that is gone (deleted or renamed in the web UI): rebuild the
      // playlist from the folder and start at its first track.
      Serial.println("file does not exist, rebuilding the playlist: " + path);
      flushPlayerCache();
      playlist.clear();
      loadAlbumDirIntoPlaylist(albumDir);
      if (playlist.size() == 0 || !card().exists(playlist.get(0))) {
        Serial.println("nothing to play in " + albumDir);
        currentlyPlayingFile = "";
        return false;
      }
      path = playlist.get(0);
      pos = 0;
    }
    handleParams(path);
    currentlyPlayingFile = path;
    bool ok = startFile(path.c_str(), pos);
    Serial.printf("playing file: %s at %u%s\n", path.c_str(), pos, podcastBusy() ? " (gated)" : "");
    return ok;
  }

  uint32_t resumePosition() override {
    uint32_t pos = audio.getFilePos();  // read position, 0 when no file is open
    if (!pos) return 0;
    uint32_t buffered = audio.inBufferFilled();  // read but not yet heard
    pos = pos > buffered ? pos - buffered : 0;
    uint32_t bitrate = audio.getBitRate();  // bit/s, 0 before the first frame
    uint32_t back = (bitrate ? bitrate : 128000) / 8 * RESUME_REWIND_S;
    uint32_t start = audio.getAudioDataStartPos();  // never continue inside the ID3 tag
    return pos > start + back ? pos - back : 0;
  }

  void loop() { audio.loop(); }

  void resume() {
    if (!audio.isRunning()) audio.pauseResume();
  }

  void pause() {
    if (audio.isRunning()) audio.pauseResume();
  }

  int getAudioCurrentTime() {
    return audio.getAudioCurrentTime();
  }

  void playClick() {
    this->songMaxVolume = 0;
    Serial.println("playing click");
    audio.stopSong();
    startFile("/tap.mp3", 0);
    unsigned long startTime = millis();
    while (millis() < startTime + 300) loop();
  }

  void playAlert() {
    this->songMaxVolume = 0;
    Serial.println("playing alert");
    audio.stopSong();
    startFile("/alert.mp3", 0);
    unsigned long startTime = millis();
    while (millis() < startTime + 4500) loop();
  }

  void printPlaylist() {
    Serial.println("index: song name");
    Serial.println("----------------");
    for (size_t i = 0; i < playlist.size(); i++) {
      audio.loop();
      String songPath = playlist.get(i);
      String songName = songPath.substring(songPath.lastIndexOf('/') + 1);
      songName.replace('_', ' ');
      Serial.print(i);
      Serial.print(": ");
      Serial.println(songName);
    }
  }
};

#endif