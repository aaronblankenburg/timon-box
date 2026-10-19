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
  void playFile(String path) { playTrack(path, 0); }

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
  bool playTrack(String path, uint32_t pos = 0) {
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