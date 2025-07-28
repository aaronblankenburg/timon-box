#ifndef PLAYERI2S_CLASS
#define PLAYERI2S_CLASS

#include <player.cpp>

#include "Audio.h"
#include "management-server.h"

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

  void setupManagementServer(String tag) {
    audio.~Audio();
    playlist.~LList();
    determineAlbumDir(tag);
    setupWebServer(this->albumDir);
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