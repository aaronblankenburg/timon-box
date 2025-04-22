#ifndef PLAYER_CLASS
#define PLAYER_CLASS

#include "Arduino.h"
#include "FS.h"
#include "LList.h"
#include "SD.h"

// SD card
#define SD_SPEED 27000000
#define SD_CS GPIO_NUM_5
#define SD_MOSI GPIO_NUM_17
#define SD_SCK GPIO_NUM_16
#define SD_MISO GPIO_NUM_4

#define LAST_PLAYED_FILE_NAME "/last_played.txt"
#define INDEX_FILE_NAME "/index.txt"

class Player {
 protected:
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

  virtual void playFile(String path) = 0;

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
  boolean sdConnected = false;
  unsigned int songMaxVolume = 0;
  String currentlyPlayingFile;

  Player() {}

  void setup() {
    startSdCard();
    startAudio();
  }

  virtual void startAudio() = 0;

  virtual void loop() = 0;

  void determineAlbumDir(String tag) {
    albumDir = "";
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
      albumDir = "/" + tag;
      Serial.println("New tag, created dir " + tag);
      SD.mkdir(albumDir);
    }
  }

  void loadPlaylist(String tag) {
    playlist.clear();
    currentlyPlayingFile = "";

    determineAlbumDir(tag);

    loadAlbumIntoPlaylist(albumDir);
  }

  void startOrContinueAlbum() {
    if (playlist.size() > 0) {
      playFile(getLastPlayedSong());
    } else {
      Serial.println("playlist empty");
    }
  }

  virtual void resume() = 0;

  virtual void pause() = 0;

  void next() {
    int currentIndex = playlistIndexOf(currentlyPlayingFile);
    if (currentIndex == playlist.size() - 1) {
      playFile(playlist.get(0));
    } else {
      playFile(playlist.get(currentIndex + 1));
    }
  }

  virtual int getAudioCurrentTime() =0;

  void previous() {
    if (getAudioCurrentTime() <= 5) {
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

    if(fileName.length() == 0) {
      Serial.println(albumDir + LAST_PLAYED_FILE_NAME + " is empty");
      return playlist.get(0);
    }

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

  virtual void playClick()=0;

  virtual void playAlert() =0;

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