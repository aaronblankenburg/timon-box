// will play many audio file formats, mp3,aac,flac etc.
// See github page : https://github.com/schreibfaul1/ESP32-audioI2S
#include <Adafruit_NeoPixel.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "Arduino.h"
#include "BluetoothA2DPSink.h"
#include "esp_bt.h"
#include "knock.cpp"
#include "playerI2S.h"

// card reader
#define CR_CS GPIO_NUM_23    // GPIO_NUM_14  // 17
#define CR_SCK GPIO_NUM_22   // 18
#define CR_MOSI GPIO_NUM_21  // 23
#define CR_MISO GPIO_NUM_19  // 19
#define CR_RST GPIO_NUM_18   // 22

#define BLUETOOTH_TAG "04c6d4b26e6780"
#define MANAGEMENT_SERVER_TAG "042041122a6781"
#define VOLUME_PIN GPIO_NUM_36
#define POWER_SUPPLY_PIN GPIO_NUM_33
#define LKS_PIN GPIO_NUM_35
#define RKS_PIN GPIO_NUM_34
#define SWITCH_TOKEN_PRESENT GPIO_NUM_32
#define VOLTAGE_PIN GPIO_NUM_39
#define NEO_PIXEL_PIN GPIO_NUM_2

#define MAX_VOLUME_LOUD 21
#define MAX_VOLUME_SAFE 17
#define LOW_BATTERY_THRESHOLD 20  // in %
#define MAX_PAUSE_TIME_IN_S 10 * 60
#define MAX_PLAY_TIME_IN_S 120 * 60
#define LEFT_AND_RIGHT_SIMULTANOUS_PRESS_TIMEOUT_IN_MS 750
#define MANAGEMENT_NO_TOKEN_TIMEOUT_IN_S 60  // management token removed but no album token placed yet (swap gap) before power-off

KnockSensor leftKnockSensor(RKS_PIN, RKS_THRESHOLD);
KnockSensor rightKnockSensor(LKS_PIN, LKS_THRESHOLD);
Adafruit_NeoPixel pixel(1, NEO_PIXEL_PIN, NEO_RGB + NEO_KHZ800);

BluetoothA2DPSink a2dp_sink;
MFRC522 rfid(CR_CS, CR_RST);
PlayerI2S player;

Preferences settings;

RTC_NOINIT_ATTR bool bluetoothMode;  // survives ESP.restart()

enum State { paused,
             playing,
             bluetoothPlaying,
             bluetoothPaused,
             managementServerRunning,     // web UI reachable via the box's own hotspot
             managementServerConnecting,  // joining the saved home network
             managementServerLan,         // web UI reachable via the home network (<box>.local)
             podcastLoading };            // podcast token: checking the feed, loading the newest episode
const uint32_t stateColors[] = {
    /* paused */ pixel.Color(255, 165, 0),                        // Orange
    /* playing */ pixel.Color(0, 255, 0),                         // Green
    /* bluetoothPlaying */ pixel.Color(0, 0, 255),                // Blue
    /* bluetoothPaused */ pixel.Color(75, 0, 130),                // Indigo
    /* managementServerRunning */ pixel.Color(255, 165, 0),       // Orange (hotspot)
    /* managementServerConnecting */ pixel.Color(255, 255, 255),  // White
    /* managementServerLan */ pixel.Color(0, 255, 255),           // Cyan (home network)
    /* podcastLoading */ pixel.Color(0, 255, 0)                   // Green, breathing
};
enum KnockSensorAction { noPress,
                         leftPress,
                         rightPress };

State state = paused;
String before_tag = "";
String recent_tag = "";
String fileWhenTagInserted = "";
unsigned long tagTimout = 100;  // ms
unsigned long musicStartTime = 0;
unsigned long musicStopTime = 0;
unsigned int tagPresentAnalogReadFilter = 0;
bool maxVolumeIsSafe = true;
unsigned long maxVolumeBoostTimeout = 2000;
unsigned long maxVolumeBoostLastRead = 0;
unsigned int maxVolume = MAX_VOLUME_SAFE;
float batteryAdcValue = 2390.;  // start with full value
String managementAlbumTag = "";        // album token currently selected in management mode
unsigned long managementNoTokenSince = 0;
unsigned long heapLogEveryMs = 0;  // serial "heap:<s>"
unsigned long lastStateSave = 0;
unsigned long podcastLoadingSince = 0;
#define PODCAST_LOADING_TIMEOUT_MS 20000  // then play what is on the card; the check carries on
#define STATE_SAVE_EVERY_MS 60000  // the position survives switching off with the power button

void setup() {
  pinMode(POWER_SUPPLY_PIN, OUTPUT);
  digitalWrite(POWER_SUPPLY_PIN, HIGH);

  esp_task_wdt_init(200, true);  // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);        // add current thread to WDT watch

  pixel.begin();                                 // Initialize the NeoPixel
  pixel.setBrightness(20);                       // Optional: 0-255
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));  // Set to off (black)
  pixel.show();                                  // Send the color data

  Serial.begin(115200);
  leftKnockSensor.begin();
  rightKnockSensor.begin();
  settings.begin("settings", false);
  restoreToneSettings();

  pinMode(VOLUME_PIN, INPUT);
  pinMode(SWITCH_TOKEN_PRESENT, INPUT_PULLUP);
  pinMode(VOLTAGE_PIN, INPUT);

  if (esp_reset_reason() != ESP_RST_SW) bluetoothMode = false;
  sdGateBegin();
  if (!bluetoothMode) {
    // The A2DP library links btStart(), so the core keeps ~55 kB reserved for Bluetooth at boot,
    // although Bluetooth only ever runs after a restart into Bluetooth mode. Give it back.
    uint32_t before = ESP.getFreeHeap();
    esp_err_t err = esp_bt_mem_release(ESP_BT_MODE_BTDM);
    Serial.printf("Bluetooth memory released (%s): free heap %u -> %u, largest block %u\n", esp_err_to_name(err), before,
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }
  if (bluetoothMode) {
    startBluetooth();
  } else {
    startPlayer();
  }
  SPI.begin(CR_SCK, CR_MISO, CR_MOSI, CR_CS);
  rfid.PCD_Init(CR_CS, CR_RST);
  rfid.PCD_SetAntennaGain(rfid.RxGain_23dB);
  batteryAdcValueManagementServer = &batteryAdcValue;
}

void loop() {
  esp_task_wdt_reset();
  checkSerialForCommands();
  updateBatteryAdc();
  if (heapLogEveryMs) {
    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog >= heapLogEveryMs) {
      lastHeapLog = millis();
      logHeap(player.audio.isRunning() ? "playing" : "idle");
    }
  }

  player.loop();

  handleVolumeBoost();

  before_tag = recent_tag == "" ? before_tag : recent_tag;
  recent_tag = getTag();

  switch ((State)state) {
    case paused:
      if (recent_tag == BLUETOOTH_TAG) {
        Serial.println("Bluetooth mode");
        if (!bluetoothMode) {
          bluetoothMode = true;
          podcastStop(true);
          ESP.restart();
        }
        changeState(bluetoothPaused);
        break;
      }

      if (recent_tag == MANAGEMENT_SERVER_TAG) {
        Serial.println("management server mode");
        podcastStop(true, 20000);  // its Wi-Fi must be down before the web server brings up its own
        player.startManagementServer();
        managementAlbumTag = "";
        managementNoTokenSince = 0;
        changeState(managementServerRunning);
        break;
      }

      if (recent_tag != "") {
        Serial.println("tag: " + recent_tag);
        if (recent_tag == before_tag && player.currentlyPlayingFile.length()) {
          player.resume();
        } else {
          checkBattery(true);
          player.audio.stopSong();  // closes the old file and frees the decoder for the feed's TLS session
          player.loadPlaylist(recent_tag);
          if (player.podcastAlbum) {
            if (getBattery() > LOW_BATTERY_THRESHOLD && podcastStart(player.albumDirectory())) {
              podcastLoadingSince = millis();
              changeState(podcastLoading);
              break;
            }
            startPodcastPlayback();  // low battery: no Wi-Fi, the newest episode on the card
          } else {
            player.startOrContinueAlbum();
          }
        }
        fileWhenTagInserted = player.currentlyPlayingFile;
        changeState(playing);
      }

      if (millis() - musicStopTime > (MAX_PAUSE_TIME_IN_S * 1000)) {
        shutdownBox();
      }
      break;

    case playing: {
      setVolume();
      leftKnockSensor.loop();
      rightKnockSensor.loop();
      // Playback that ends without a track end (audio_eof_mp3 starts the next one right away) or a
      // pause is worth knowing about: log where and with how much memory.
      static bool wasRunning = false;
      static uint32_t lastPos = 0;
      bool running = player.audio.isRunning();
      if (running) lastPos = player.audio.getFilePos();
      if (wasRunning && !running && recent_tag != "") {
        Serial.printf("playback stopped by itself: %s at %u, heap %u, largest %u\n", player.currentlyPlayingFile.c_str(),
                      lastPos, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
      }
      wasRunning = running;

      if (recent_tag == "") {
        musicStopTime = millis();
        player.pause();
        player.savePlayingState();
        changeState(paused);
        delay(3000);
      } else if (millis() - lastStateSave > STATE_SAVE_EVERY_MS) {
        lastStateSave = millis();
        // Only if the card is free right now: a podcast download may be writing to it.
        if (player.audio.isRunning() && sdGateTake(0)) {
          player.savePlayingState(true);
          sdGateGive();
        }
      }

      if (millis() - musicStartTime > (MAX_PLAY_TIME_IN_S * 1000)) {
        player.pause();
        player.savePlayingState();
        shutdownBox();
      }
      break;
    }

    case podcastLoading:
      pulseLed(true);
      if (recent_tag == "") {
        // Taken off before anything played: the check carries on in the background, and putting
        // the token back counts as a new start.
        before_tag = "";
        musicStopTime = millis();
        pulseLed(false);
        changeState(paused);
        break;
      }
      // Ready, or finished one way or another (up to date, offline, failed), or waited long enough.
      if (podcastPlayable() || podcastPhase > POD_DOWNLOADING ||
          millis() - podcastLoadingSince > PODCAST_LOADING_TIMEOUT_MS) {
        pulseLed(false);
        startPodcastPlayback();
        fileWhenTagInserted = player.currentlyPlayingFile;
        changeState(playing);
      }
      break;

    case bluetoothPlaying:
      leftKnockSensor.loop();
      rightKnockSensor.loop();

      if (recent_tag == "") {
        if (bluetoothMode) {
          bluetoothMode = false;
          ESP.restart();
        }
        changeState(paused);
      }
      break;

    case bluetoothPaused:
      if (recent_tag == "") {
        if (bluetoothMode) {
          bluetoothMode = false;
          ESP.restart();
        }
        changeState(paused);
      }
      if (millis() - musicStopTime > (MAX_PAUSE_TIME_IN_S * 1000)) {
        shutdownBox();
      }
      break;

    case managementServerRunning:
    case managementServerConnecting:
    case managementServerLan: {
      if (recent_tag == "") {
        if (managementAlbumTag != "") {
          // Album token removed: we are done, power off right away (as before).
          shutdownBox();
          changeState(paused);
          ESP.restart();
        } else if (managementNoTokenSince == 0) {
          // Management token removed: allow time to swap in the album token.
          managementNoTokenSince = millis();
        } else if (millis() - managementNoTokenSince > (MANAGEMENT_NO_TOKEN_TIMEOUT_IN_S * 1000)) {
          shutdownBox();
          changeState(paused);
          ESP.restart();
        }
      } else {
        managementNoTokenSince = 0;
        if (recent_tag != MANAGEMENT_SERVER_TAG && recent_tag != managementAlbumTag) {
          // Album token placed: manage its files (Files/Upload appear in the web UI)
          managementAlbumTag = recent_tag;
          player.selectAlbumForManagement(recent_tag);
        }
      }
      NetMode netMode = managementServerLoop();
      changeState(netMode == NET_STATION ? managementServerLan
                  : netMode == NET_CONNECTING ? managementServerConnecting
                                              : managementServerRunning);
      updateTransferLed();
      delay(1);  // nothing here is time critical; give the WiFi and web server tasks the core
      break;
    }
  }

  if (!player.sdConnected && (state == playing || state == paused)) {
    Serial.println("SD-Card Failure, rebooting!");
    ESP.restart();
  }
}

// The colour the LED shows at rest: the state colour, or red on a low battery.
uint32_t stateColor() {
  return getBattery() > LOW_BATTERY_THRESHOLD ? stateColors[state] : pixel.Color(255, 0, 0);
}

void changeState(State newState) {
  if (state == newState) return;

  state = newState;
  setNeoPixel(stateColor());
}

uint32_t scaleColor(uint32_t color, uint8_t percent) {
  uint8_t r = (uint8_t)(color >> 16), g = (uint8_t)(color >> 8), b = (uint8_t)color;
  return pixel.Color(r * percent / 100, g * percent / 100, b * percent / 100);
}

// While the other box pulls tracks from us, breathe the state colour so it is
// obvious that this box is busy.
void updateTransferLed() { pulseLed(peerTransferActive()); }

// Breathes the state colour while `active`, steady otherwise. Triangle wave, 25 % to 100 %,
// ~1.2 s period.
void pulseLed(bool active) {
  static bool pulsing = false;
  static unsigned long lastFrame = 0;

  if (!active) {
    if (pulsing) {
      pulsing = false;
      setNeoPixel(stateColor());  // back to a steady light
    }
    return;
  }
  pulsing = true;
  if (millis() - lastFrame < 40) return;  // 25 frames per second is plenty
  lastFrame = millis();

  unsigned long phase = millis() % 1200;
  uint8_t level = phase < 600 ? 25 + (phase * 75) / 600 : 100 - ((phase - 600) * 75) / 600;
  setNeoPixel(scaleColor(stateColor(), level));
}

void setNeoPixel(uint32_t color) {
  pixel.setPixelColor(0, color);
  pixel.show();
  pixel.show();
  pixel.show();
}

void startPlayer() {
  player.setup();
  rightKnockSensor.action = []() { handleKnockSensorActionPlayer(rightPress); };
  leftKnockSensor.action = []() { handleKnockSensorActionPlayer(leftPress); };
}

void startBluetooth() {
  i2s_pin_config_t my_pin_config = {.bck_io_num = 27,
                                    .ws_io_num = 26,
                                    .data_out_num = 25,
                                    .data_in_num = I2S_PIN_NO_CHANGE};
  static i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 16000,  // updated automatically by A2DP
      .bits_per_sample = (i2s_bits_per_sample_t)16,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 1024,
      .use_apll = false,
      .tx_desc_auto_clear =
          true  // avoiding noise in case of data unavailability
  };
  a2dp_sink.set_pin_config(my_pin_config);
  a2dp_sink.set_i2s_config(i2s_config);
  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.start(BOX_NAME);
  a2dp_sink.set_on_connection_state_changed(
      [](esp_a2d_connection_state_t connectionState, void *ptr) {
        if (connectionState == ESP_A2D_CONNECTION_STATE_CONNECTED) {
          a2dp_sink.play();
          changeState(bluetoothPlaying);
        } else if (connectionState == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
          changeState(bluetoothPaused);
          musicStopTime = millis();
        }
      });
  a2dp_sink.set_on_audio_state_changed([](esp_a2d_audio_state_t audioState, void *ptr) {
    if (audioState == ESP_A2D_AUDIO_STATE_STARTED) {
      changeState(bluetoothPlaying);
    } else if (audioState == ESP_A2D_AUDIO_STATE_STOPPED || audioState == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND) {
      changeState(bluetoothPaused);
      musicStopTime = millis();
    }
  });
  // rightKnockSensor.action = []() { a2dp_sink.next(); };
  // leftKnockSensor.action = []() { a2dp_sink.previous(); };
  rightKnockSensor.action = []() { handleKnockSensorActionBluetooth(rightPress); };
  leftKnockSensor.action = []() { handleKnockSensorActionBluetooth(leftPress); };
}

KnockSensorAction lastKnockSensorAction = noPress;
unsigned long long lastKnockSensorActionTimestamp;
void handleKnockSensorActionPlayer(KnockSensorAction action) {
  maxVolume = maxVolumeIsSafe ? MAX_VOLUME_SAFE : MAX_VOLUME_LOUD;
  setVolume();
  player.playClick();
  bool actionChanged = (lastKnockSensorAction == leftPress && action == rightPress) || (lastKnockSensorAction == rightPress && action == leftPress);
  bool leftAndRightPressed = millis() - lastKnockSensorActionTimestamp < LEFT_AND_RIGHT_SIMULTANOUS_PRESS_TIMEOUT_IN_MS && actionChanged;
  if (leftAndRightPressed) {
    player.playSongByIndex(0);
  } else {
    if (action == leftPress) {
      player.previous();
    }
    if (action == rightPress) {
      player.next();
    }
  }
  fileWhenTagInserted = player.currentlyPlayingFile;
  lastKnockSensorAction = action;
  lastKnockSensorActionTimestamp = millis();
}

void handleKnockSensorActionBluetooth(KnockSensorAction action) {
  if (action == leftPress) {
    a2dp_sink.previous();
  }
  if (action == rightPress) {
    a2dp_sink.next();
  }
  lastKnockSensorAction = action;
  lastKnockSensorActionTimestamp = millis();
}

String getTag() {
  if (!isTokenPresent()) {
    return "";
  }

  if (recent_tag != "") {
    return recent_tag;
  }

  if (millis() - musicStartTime < tagTimout) {
    return recent_tag;
  } else
    musicStartTime = millis();

  Serial.println("Read tag");

  rfid.PCD_SoftPowerUp();

  if (!rfid.PICC_IsNewCardPresent()) {
    return "";
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return recent_tag;
  }

  char hexstr[(rfid.uid.size * 2) + 1];
  int i;
  for (i = 0; i < rfid.uid.size; i++) {
    sprintf(hexstr + i * 2, "%02x", rfid.uid.uidByte[i]);
  }
  hexstr[i * 2] = 0;

  rfid.PICC_IsNewCardPresent();  // flush card

  rfid.PCD_SoftPowerDown();
  return hexstr;
}

void audio_eof_mp3(const char *info) {  // end of file; `info` is the file name without its folder
  // The click and the low-battery alert end inside their own wait loops (playClick/playAlert).
  // Moving on here skipped a track, and the alert's end called checkBattery() recursively.
  if (strcmp(info, "tap.mp3") == 0 || strcmp(info, "alert.mp3") == 0) return;
  checkBattery(true);
  if (player.podcastAlbum) {
    // From the newest episode back to the older ones; after the oldest the box switches off.
    int older = player.olderEpisodeIndex(player.currentIndex());
    if (older >= 0) {
      player.playSongByIndex(older);
    } else {
      player.savePlayingState();
      shutdownBox();
    }
    return;
  }
  player.next();
  if (fileWhenTagInserted == player.currentlyPlayingFile) {
    // shutdown when starting file is reached again; the next time the album starts here, from the top
    fileWhenTagInserted = "";
    player.savePlayingState();
    shutdownBox();
  }
}

// Podcast albums start with the newest episode: the one the podcast task just checked or
// downloaded, or, without network, the newest on the card. If that is the one heard last, it
// continues at the remembered spot.
void startPodcastPlayback() {
  player.reloadPlaylist();  // the task may have added and removed episodes
  String path = podcastEpisode();
  if (path.isEmpty() || player.indexOfTrack(path) < 0) {
    int newest = player.newestEpisodeIndex();
    if (newest < 0) {
      Serial.println("podcast: no episode on the card yet");
      player.startOrContinueAlbum();  // whatever else is in the folder
      return;
    }
    path = player.trackAt(newest);
  }
  String last = player.getLastPlayedSong();
  uint32_t pos = last == path ? player.lastPosition() : 0;
  uint32_t synced, expected;
  bool done;
  if (pos && growingGet(path.c_str(), synced, expected, done) && !done) {
    // A resume searches the next frame byte by byte: keep it well inside what is on the card.
    pos = synced > 2 * PODCAST_FIRST_FLUSH ? min(pos, synced - PODCAST_FIRST_FLUSH) : 0;
  }
  player.playTrack(path, pos);
}

void setVolume() {
  player.audio.setVolume(::map(analogRead(VOLUME_PIN), 0, 4095, 2, maxVolume));
}

bool isTokenPresent() {
  return digitalRead(SWITCH_TOKEN_PRESENT) == LOW;
}

void handleVolumeBoost() {
  maxVolume = maxVolumeIsSafe ? player.songMaxVolume > 0 &&
                                        player.songMaxVolume <= MAX_VOLUME_LOUD
                                    ? player.songMaxVolume
                                    : MAX_VOLUME_SAFE
                              : MAX_VOLUME_LOUD;
}

void checkBattery(bool playAlert) {
  int batteryPercent = getBattery();
  Serial.println("Battery level is " + String(batteryPercent) + "%");
  if (batteryPercent <= LOW_BATTERY_THRESHOLD) {
    if (playAlert) {
      player.playAlert();
    }
    setNeoPixel(pixel.Color(255, 0, 0));
  }
}

unsigned int getBattery() {
  // unsigned int batteryPercent = ::map(value, 400, 3100, 0, 100);
  unsigned int batteryPercent = constrain(::map(::map((uint)batteryAdcValue, 1994, 2390, 3685, 4322), 3200, 4200, 0, 100), 0, 100);
  return batteryPercent;
}

void updateBatteryAdc() {
  // Sample every 10 ms instead of on every loop iteration: analogRead() at several kHz keeps
  // toggling the SAR ADC power domain that the WiFi PHY shares. Filter time constant ~0.5 s.
  static unsigned long lastSample = 0;
  if (millis() - lastSample < 10) return;
  lastSample = millis();
  batteryAdcValue = (batteryAdcValue * 0.98) + (0.02 * analogRead(VOLTAGE_PIN));
}

void checkSerialForCommands() {
  if (Serial.available() == 0) return;
  String cmd = Serial.readStringUntil(':');
  cmd.trim();
  cmd.toLowerCase();
  String value = Serial.readStringUntil('\n');

  if (cmd == "low" || cmd == "mid" || cmd == "high") {
    value.trim();
    if (value == "") {
      Serial.println("Read \"" + cmd +
                     "\": " + settings.getInt(cmd.c_str(), 0));
    } else {
      int intVal = constrain(value.toInt(), -40, 6);

      settings.putInt(cmd.c_str(), intVal);
      Serial.println("Set \"" + cmd + "\" to " + intVal);

      restoreToneSettings();
    }
  } else if (cmd == "list") {
    Serial.println("use \"play:<index>\" to play a specific song");
    player.printPlaylist();
  } else if (cmd == "play") {
    value.trim();
    player.playSongByIndex(value.toInt());
  } else if (cmd == "maxvolume") {
    value.trim();
    value.toLowerCase();
    if (value == "safe") maxVolumeIsSafe = true;
    if (value == "unsafe") maxVolumeIsSafe = false;
    Serial.println("Set maxVolume to " + maxVolumeIsSafe ? "safe" : "unsafe");
  } else if (cmd == "heap") {
    value.trim();
    heapLogEveryMs = value.toInt() * 1000UL;
    logHeap("now");
  } else if (cmd == "dl") {
    value.trim();
    if (podcastDebugDownload(value)) Serial.println("downloading to " + podcastDebugPath);
    else Serial.println("a download is already running");
  } else if (cmd == "dltls") {
    value.trim();
    podcastPreferHttp = value.toInt() == 0;
    Serial.println(podcastPreferHttp ? "episodes over plain http where possible" : "episodes always over https");
  } else if (cmd == "podcasturl") {
    value.trim();
    PodcastConfig cfg;
    cfg.url = value;
    bool ok = player.albumDirectory().length() && writePodcastConfig(player.albumDirectory(), cfg);
    Serial.printf("podcast.txt in %s: %s\n", player.albumDirectory().c_str(), ok ? value.c_str() : "not written");
  } else if (cmd == "dlstop") {
    podcastCancel = true;
  } else if (cmd == "dlplay") {
    value.trim();
    player.playTrack(value.length() ? value : podcastDebugPath, 0);
    fileWhenTagInserted = player.currentlyPlayingFile;
  } else if (cmd == "playat") {  // test: continue the current track at a byte position
    value.trim();
    player.playTrack(player.currentlyPlayingFile, strtoul(value.c_str(), nullptr, 10));
  } else if (cmd == "restart") {
    podcastStop(true);
    ESP.restart();
  } else if (cmd == "audio") {
    Serial.printf("audio: %s, %s, at %u of %u, %u buffered, %u kbit/s, %u s\n", player.audio.isRunning() ? "running" : "not running",
                  player.currentlyPlayingFile.length() ? player.currentlyPlayingFile.c_str() : "-", player.audio.getFilePos(),
                  player.audio.getFileSize(), player.audio.inBufferFilled(), player.audio.getBitRate() / 1000,
                  player.audio.getAudioCurrentTime());
  } else if (cmd == "podcast") {
    uint32_t synced = 0, expected = 0;
    bool done = false;
    String episode = podcastEpisode();
    bool grows = episode.length() && growingGet(episode.c_str(), synced, expected, done);
    Serial.printf("podcast: %s%s, album %s, episode %s", podcastPhaseName(podcastPhase), podcastBusy() ? " (running)" : "",
                  player.podcastAlbum ? "is a podcast" : "is not a podcast", episode.length() ? episode.c_str() : "-");
    if (grows) Serial.printf(", %u of %u bytes on the card%s", synced, expected, done ? ", done" : "");
    Serial.println();
    logHeap("now");
  } else if (cmd == "md5") {
    value.trim();
    uint32_t size;
    String sum = fileMd5(value, size);
    Serial.printf("md5 %s %s (%u bytes)\n", sum.c_str(), value.c_str(), size);
  } else if (cmd == "knockdebug") {
    value.trim();
    int level = constrain(value.toInt(), 0, 2);
    leftKnockSensor.debug = rightKnockSensor.debug = level;
    Serial.println(level == 0 ? "knock statistics off" : level == 1 ? "knock statistics on" : "knock statistics on, tracing the next hump");
  } else {
    Serial.println("Command \"" + cmd +
                   "\" is not supported. Supported commands: low[:<-40..6>], "
                   "mid[:<-40..6>], high[:<-40..6>], list, play:<index>, "
                   "maxVolume:<safe|unsafe>, knockDebug:<0|1|2>, heap[:<seconds>], dl:<url>, dlstop, "
                   "dlplay[:<path>], md5:<path>, podcast, audio, dltls:<0|1>, podcasturl:<url>, playat:<byte>, restart");
  }
  Serial.readString();
}

void restoreToneSettings() {
  player.audio.setTone(settings.getInt("low", 0), settings.getInt("mid", 0),
                       settings.getInt("high", 0));
}

void shutdownBox() {
  podcastStop(true);  // close the episode file before the power goes
  // On USB power the supply stays on and the callers keep calling this every loop iteration:
  // say it once, or the console drowns in it.
  static bool announced = false;
  if (!announced) Serial.println("shutting down");
  announced = true;
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));  // Set to off (black)
  digitalWrite(POWER_SUPPLY_PIN, LOW);
}