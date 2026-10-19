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
  void playFile(String path) {
    // startSdCard();
    if (SD.exists(path)) {
      handleParams(path);
      currentlyPlayingFile = path;
      sdConnected = audio.connecttoFS(SD, path.c_str());
      Serial.println("playing file: " + path);
    } else {
      Serial.println("file does not exist, flushing player cache: " + path);
      flushPlayerCache();
      loadAlbumDirIntoPlaylist(albumDir);
      startOrContinueAlbum();
    }
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

  // Starts `path` at byte `pos` (0 = from the start). While a podcast download runs, every file
  // goes through gatedFS, so the decoder never waits for the card the downloader writes to.
  bool playTrack(const String &path, uint32_t pos = 0) {
    handleParams(path);
    currentlyPlayingFile = path;
    fs::FS &card = podcastBusy() ? gatedFS : (fs::FS &)SD;
    bool ok = audio.connecttoFS(card, path.c_str(), pos);
    Serial.printf("playing file: %s%s at %u\n", path.c_str(), podcastBusy() ? " (gated)" : "", pos);
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
    sdConnected = audio.connecttoFS(SD, "/tap.mp3");
    unsigned long startTime = millis();
    while (millis() < startTime + 300) loop();
  }

  void playAlert() {
    this->songMaxVolume = 0;
    Serial.println("playing alert");
    audio.stopSong();
    sdConnected = audio.connecttoFS(SD, "/alert.mp3");
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