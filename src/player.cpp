#ifndef PLAYER_CLASS
#define PLAYER_CLASS

#include "Arduino.h"
#include "FS.h"
#include "LList.h"
#include "SD.h"
#include "podcast-feed.h"

// SD card
#define SD_SPEED 27000000
#define SD_CS GPIO_NUM_5
#define SD_MOSI GPIO_NUM_17
#define SD_SCK GPIO_NUM_16
#define SD_MISO GPIO_NUM_4

#define LAST_PLAYED_FILE_NAME "/last_played.txt"
#define INDEX_FILE_NAME "/index.txt"
#define RESUME_REWIND_S 5  // continue a track this many seconds before where it was left

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
    // At most 5 files open through the VFS. Each one reserves a 4 kB sector buffer for good,
    // and the podcast downloader does not count: it uses FATFS directly (see podcast.h).
    if (!SD.begin(SD_CS, spiSD, SD_SPEED, "/sd", 5)) {
      Serial.println("Error talking to SD card!");
      sdConnected = false;
    } else {
      sdConnected = true;
    }
  }

  virtual bool playTrack(String path, uint32_t pos) = 0;
  void playFile(String path) { playTrack(path, 0); }

  // Where to continue the current track (already RESUME_REWIND_S back), 0 = from the start.
  virtual uint32_t resumePosition() = 0;

  uint32_t lastPlayedPos = 0;  // from the second line of last_played.txt

  void flushPlayerCache() {
    if (SD.exists(albumDir + LAST_PLAYED_FILE_NAME)) SD.remove(albumDir + LAST_PLAYED_FILE_NAME);
    if (SD.exists(albumDir + INDEX_FILE_NAME)) SD.remove(albumDir + INDEX_FILE_NAME);
  }

  /**
   * handle params in filename submitted as param[value]
   * supported param: vol with value between 0 and 21
   */
  void handleParams(String path) {
    int start = path.indexOf("vol[");
    int end = start >= 0 ? path.indexOf(']', start) : -1;
    if (end > start + 4) {
      this->songMaxVolume = path.substring(start + 4, end).toInt();
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
    Serial.println("Saving playlist with size " + String(playlist.size()));
    for (size_t i = 0; i < playlist.size(); i++) {
      file.println(playlist.get(i));
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
      // Serial.print(file.name());
      // Serial.print("  ");
      // Serial.print(String(file.name()));
      // Serial.print("  ");
      // Serial.println(file.isDirectory());
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
    podcastAlbum = SD.exists(albumDir + "/podcast.txt");

    loadAlbumIntoPlaylist(albumDir);
  }

  // The album's playlist again, e.g. after the podcast task added or removed episodes.
  void reloadPlaylist() {
    playlist.clear();
    loadAlbumIntoPlaylist(albumDir);
  }

  bool podcastAlbum = false;  // the album folder has a podcast.txt
  uint32_t lastPosition() const { return lastPlayedPos; }  // set by getLastPlayedSong()
  const String &albumDirectory() const { return albumDir; }
  int currentIndex() { return playlistIndexOf(currentlyPlayingFile); }
  int indexOfTrack(const String &path) { return playlistIndexOf(path); }
  String trackAt(int index) { return playlist.get(index); }

  static bool isEpisodePath(const String &path) {
    return isEpisodeFileName(path.c_str() + path.lastIndexOf('/') + 1);
  }

  // Episodes sort by date, so the newest is the last one (uploaded files are not episodes).
  int newestEpisodeIndex() {
    for (int i = (int)playlist.size() - 1; i >= 0; i--) {
      if (isEpisodePath(playlist.get(i))) return i;
    }
    return -1;
  }

  int olderEpisodeIndex(int index) {
    for (int i = index - 1; i >= 0; i--) {
      if (isEpisodePath(playlist.get(i))) return i;
    }
    return -1;
  }

  void startOrContinueAlbum() {
    if (playlist.size() > 0) {
      String track = getLastPlayedSong();
      playTrack(track, lastPlayedPos);
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

  // last_played.txt holds the track's path and, on a second line, the byte position to continue
  // from. Without that line (older firmware, "Play next" in the web UI) the track starts at 0.
  // Sets lastPlayedPos.
  String getLastPlayedSong() {
    lastPlayedPos = 0;
    if (!SD.exists(albumDir + LAST_PLAYED_FILE_NAME)) {
      Serial.println("Failed to open file for reading");
      return playlist.get(0);
    }

    File file = SD.open(albumDir + LAST_PLAYED_FILE_NAME);

    String content;
    while (file.available()) {
      content += (char)file.read();
    }
    file.close();

    int newline = content.indexOf('\n');
    String fileName = newline >= 0 ? content.substring(0, newline) : content;
    fileName.trim();
    uint32_t pos = newline >= 0 ? strtoul(content.c_str() + newline + 1, nullptr, 10) : 0;

    if (fileName.length() == 0) {
      Serial.println(albumDir + LAST_PLAYED_FILE_NAME + " is empty");
      return playlist.get(0);
    }

    File track = SD.open(fileName);
    if (!track) {
      return playlist.get(0);
    }
    if (pos < track.size()) lastPlayedPos = pos;  // a replaced, shorter file starts over
    track.close();
    return fileName;
  }

  void savePlayingState(bool quiet = false) {
    File file = SD.open(albumDir + LAST_PLAYED_FILE_NAME, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
    uint32_t pos = resumePosition();
    bool ok = file.print(currentlyPlayingFile);
    if (ok && pos) ok = file.print('\n') && file.print(pos);
    if (ok && !quiet) {
      Serial.printf("Last played song saved: %s at %u\n", currentlyPlayingFile.c_str(), pos);
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