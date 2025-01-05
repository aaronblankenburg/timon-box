#ifndef PLAYER_CLASS
#define PLAYER_CLASS

#include "Arduino.h"
#include "Audio.h"
#include "FS.h"
#include "LList.h"
#include "SD.h"

#include "management-server.h"

// SD card
#define SD_SPEED 27000000
#define SD_CS GPIO_NUM_5
#define SD_MOSI GPIO_NUM_17
#define SD_SCK GPIO_NUM_16
#define SD_MISO GPIO_NUM_4

#define I2S_DOUT 25
#define I2S_BCLK GPIO_NUM_27  // I2S
#define I2S_LRC GPIO_NUM_26

#define LAST_PLAYED_FILE_NAME "/last_played.txt"
#define INDEX_FILE_NAME "/index.txt"


class Player {
 private:
  SPIClass spiSD = SPIClass(HSPI);
  String albumDir;
  LList<String> playlist;

  int playlistIndexOf(String track) {
    for (size_t i = 0; i < playlist.size(); i++) {
      if (playlist.get(i) == track) return i;
    }
    return -1;
  }

  void startSdCard() {
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, spiSD, SD_SPEED)) {
      Serial.println("Error talking to SD card!");
      sdConnected = false;
    } else {
      sdConnected = true;
    }
  }

  void playFile(String path) {
    // startSdCard();
    if (SD.exists(path)) {
      handleParams(path);
      currentlyPlayingFile = path;
      sdConnected = audio.connecttoSD(path.c_str());
      Serial.println("playing file: " + path);
    } else {
      Serial.println("file does not exist, flushing player cache: " + path);
      flushPlayerCache();
      loadAlbumDirIntoPlaylist(albumDir);
      startOrContinueAlbum();
    }
  }

  void flushPlayerCache() {
    if (SD.exists(albumDir + LAST_PLAYED_FILE_NAME)) SD.remove(albumDir + LAST_PLAYED_FILE_NAME);
    if (SD.exists(albumDir + INDEX_FILE_NAME)) SD.remove(albumDir + INDEX_FILE_NAME);
  }

  /**
   * handle params in filename submitted as param[value]
   * supported param: vol with value between 0 and 21
   */
  void handleParams(String path) {
    int maxVolStart = path.indexOf("vol[") + 4;

    if (maxVolStart > -1) {
      String m = path.substring(maxVolStart);
      this->songMaxVolume = m.substring(0, m.indexOf("]")).toInt();
      Serial.print("Set songMaxVolume to ");
      Serial.println(this->songMaxVolume);
    } else {
      this->songMaxVolume = 0;
    }
  }

  void loadAlbumIntoPlaylist(String albumDir) {
    String indexFilePath = albumDir + INDEX_FILE_NAME;
    if (SD.exists(indexFilePath)) {
      Serial.println("Load index file");
      loadAlbumIndexIntoPlaylist(indexFilePath);
    } else {
      Serial.println("Load album from dir and create index file");
      loadAlbumDirIntoPlaylist(albumDir);
      savePlaylistToAlbumIndex(indexFilePath);
    }
  }

  void loadAlbumIndexIntoPlaylist(String indexFilePath) {
    File file = SD.open(albumDir + INDEX_FILE_NAME);

    String fileName;
    while (file.available()) {
      fileName = file.readStringUntil('\n');
      fileName.trim();
      playlist.add(fileName);
    }
    file.close();

    if (playlist.size() == 0) {
      Serial.println("Index empty, fallback to read from dir");
      loadAlbumDirIntoPlaylist(albumDir);
      savePlaylistToAlbumIndex(indexFilePath);
    }
  }

  void savePlaylistToAlbumIndex(String indexFilePath) {
    File file = SD.open(indexFilePath, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open album index file for writing");
      return;
    }
    Serial.println("Saving playlist with size " + playlist.size());
    for (uint8_t i = 0; i < playlist.size(); i++) {
      file.println(playlist[i]);
    }
    Serial.println("Index saved to " + indexFilePath);
    file.close();
  }

  void loadAlbumDirIntoPlaylist(String albumDir) {
    File album = SD.open(albumDir);
    File file = album.openNextFile();

    while (file) {
      if (!file.isDirectory() && String(file.name()).endsWith(".mp3")) {
        playlist.add(albumDir + "/" + file.name());
      }
      file = album.openNextFile();
    }
    playlist.sort([](String &a, String &b) { return a.compareTo(b); });
  }

 public:
  Audio audio = Audio();
  boolean sdConnected = false;
  unsigned int songMaxVolume = 0;
  String currentlyPlayingFile;

  Player() {}

  void setup() {
    startSdCard();
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.forceMono(true);
  }

  void setupManagementServer(){
    setupWebServer(this->albumDir);
  }

  void loop() { audio.loop(); }

  void loadPlaylist(String tag) {
    albumDir = "";
    playlist.clear();
    currentlyPlayingFile = "";

    File root = SD.open("/");
    File file = root.openNextFile();
    while (file) {
      Serial.print(file.name());
      Serial.print("  ");
      Serial.print(String(file.name()));
      Serial.print("  ");
      Serial.println(file.isDirectory());
      if (file.isDirectory() && String(file.name()).endsWith(tag)) {
        albumDir = file.name();
        albumDir = "/" + albumDir;
        break;
      }
      file = root.openNextFile();
    }

    Serial.println("albumDir: " + albumDir);

    if (albumDir == "") {
      Serial.println("New tag, created dir /" + tag);
      SD.mkdir("/" + tag);
      return;
    }

    loadAlbumIntoPlaylist(albumDir);
  }

  void startOrContinueAlbum() {
    if (playlist.size() > 0) {
      playFile(getLastPlayedSong());
    } else {
      Serial.println("playlist empty");
    }
  }

  void resume() {
    if (!audio.isRunning()) audio.pauseResume();
  }

  void pause() {
    if (audio.isRunning()) audio.pauseResume();
  }

  void next() {
    int currentIndex = playlistIndexOf(currentlyPlayingFile);
    if (currentIndex == playlist.size() - 1) {
      playFile(playlist.get(0));
    } else {
      playFile(playlist.get(currentIndex + 1));
    }
  }

  void previous() {
    if (audio.getAudioCurrentTime() <= 5) {
      int currentIndex = playlistIndexOf(currentlyPlayingFile);
      if (currentIndex == 0) {
        playFile(playlist.get(playlist.size() - 1));
      } else {
        playFile(playlist.get(currentIndex - 1));
      }
    } else {
      // after 5 sec just restart file
      playFile(currentlyPlayingFile);
    }
  }

  String getLastPlayedSong() {
    if (!SD.exists(albumDir + LAST_PLAYED_FILE_NAME)) {
      Serial.println("Failed to open file for reading");
      return playlist.get(0);
    }

    File file = SD.open(albumDir + LAST_PLAYED_FILE_NAME);

    String fileName;
    while (file.available()) {
      fileName += (char)file.read();
    }
    file.close();

    if (SD.exists(fileName)) {
      return fileName;
    } else {
      return playlist.get(0);
    }
  }

  void savePlayingState() {
    File file = SD.open(albumDir + LAST_PLAYED_FILE_NAME, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
    if (file.print(currentlyPlayingFile)) {
      Serial.println("Last played song saved");
    }
    file.close();
  }

  void playClick() {
    this->songMaxVolume = 0;
    Serial.println("playing click");
    audio.stopSong();
    sdConnected = audio.connecttoSD("/tap.mp3");
    unsigned long startTime = millis();
    while (millis() < startTime + 300) loop();
  }

  void playAlert() {
    this->songMaxVolume = 0;
    Serial.println("playing alert");
    audio.stopSong();
    sdConnected = audio.connecttoSD("/alert.mp3");
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

  void playSongByIndex(int index) {
    if (index < 0) {
      Serial.println("index < 0");
      return;
    }
    if (index >= playlist.size()) {
      Serial.println("index too large");
      return;
    }

    playFile(playlist.get(index));
  }
};

#endif