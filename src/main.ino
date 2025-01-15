// will play many audio file formats, mp3,aac,flac etc.
// See github page : https://github.com/schreibfaul1/ESP32-audioI2S
#include <MFRC522.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "Arduino.h"
#include "BluetoothA2DPSink.h"
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
#define VOLUME_BOOST_PIN GPIO_NUM_12
#define VOLTAGE_PIN GPIO_NUM_39

#define MAX_VOLUME_LOUD 21
#define MAX_VOLUME_SAFE 13
#define LOW_BATTERY_THRESHOLD 10  // in %
#define MAX_PAUSE_TIME_IN_S 10 * 60
#define MAX_PLAY_TIME_IN_S 120 * 60
#define LEFT_AND_RIGHT_SIMULTANOUS_PRESS_TIMEOUT_IN_MS 750

KnockSensor leftKnockSensor(RKS_PIN, 30000);
KnockSensor rightKnockSensor(LKS_PIN, 40000);

BluetoothA2DPSink a2dp_sink;
MFRC522 rfid(CR_CS, CR_RST);
PlayerI2S player;

Preferences settings;

RTC_NOINIT_ATTR bool bluetoothMode;  // survives ESP.restart()

enum State { paused,
             playing,
             bluetoothPlaying,
             bluetoothPaused,
             managementServerWaitForTag,
             managementServerRunning };
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

void setup() {
  pinMode(POWER_SUPPLY_PIN, OUTPUT);
  digitalWrite(POWER_SUPPLY_PIN, HIGH);
  esp_task_wdt_init(200, true);  // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);        // add current thread to WDT watch
  Serial.begin(115200);
  settings.begin("settings", false);
  restoreToneSettings();

  pinMode(VOLUME_PIN, INPUT);
  pinMode(VOLUME_BOOST_PIN, INPUT_PULLUP);
  pinMode(VOLTAGE_PIN, INPUT);

  if (esp_reset_reason() != ESP_RST_SW) bluetoothMode = false;
  if (bluetoothMode) {
    startBluetooth();
  } else {
    startPlayer();
  }
  SPI.begin(CR_SCK, CR_MISO, CR_MOSI, CR_CS);
  rfid.PCD_Init(CR_CS, CR_RST);
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
}

void loop() {
  esp_task_wdt_reset();
  checkSerialForCommands();

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
          ESP.restart();
        }
        state = bluetoothPaused;
        break;
      }

      if (recent_tag == MANAGEMENT_SERVER_TAG) {
        Serial.println("management server mode, waiting for token");
        state = managementServerWaitForTag;
        break;
      }

      if (recent_tag != "") {
        Serial.println("tag: " + recent_tag);
        if (recent_tag == before_tag) {
          player.resume();
        } else {
          checkBattery(true);
          player.loadPlaylist(recent_tag);
          player.startOrContinueAlbum();
        }
        fileWhenTagInserted = player.currentlyPlayingFile;
        state = playing;
      }

      if (millis() - musicStopTime > (MAX_PAUSE_TIME_IN_S * 1000)) {
        shutdown();
      }
      break;

    case playing:
      setVolume();
      leftKnockSensor.loop();
      rightKnockSensor.loop();

      if (recent_tag == "") {
        musicStopTime = millis();
        player.pause();
        player.savePlayingState();
        state = paused;
        delay(3000);
      }

      if (millis() - musicStartTime > (MAX_PLAY_TIME_IN_S * 1000)) {
        player.pause();
        player.savePlayingState();
        shutdown();
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
        state = paused;
      }
      break;

    case bluetoothPaused:
      if (recent_tag == "") {
        if (bluetoothMode) {
          bluetoothMode = false;
          ESP.restart();
        }
        state = paused;
      }
      if (millis() - musicStopTime > (MAX_PAUSE_TIME_IN_S * 1000)) {
        shutdown();
      }
      break;

    case managementServerWaitForTag:
      if (recent_tag != "" && recent_tag != MANAGEMENT_SERVER_TAG) {
        player.setupManagementServer(recent_tag);
        state = managementServerRunning;
      }
      break;

    case managementServerRunning:
      if (recent_tag == "") {
        shutdown();
        state = paused;
        ESP.restart();
      }
      dnsServer.processNextRequest();
      break;
  }

  if (!player.sdConnected && (state == playing || state == paused)) {
    Serial.println("SD-Card Failure, rebooting!");
    ESP.restart();
  }
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
  a2dp_sink.start("Timon-Box");
  a2dp_sink.set_on_connection_state_changed(
      [](esp_a2d_connection_state_t connectionState, void *ptr) {
        if (connectionState == ESP_A2D_CONNECTION_STATE_CONNECTED) {
          a2dp_sink.play();
          state = bluetoothPlaying;
        } else if (connectionState == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
          state = bluetoothPaused;
          musicStopTime = millis();
        }
      });
  a2dp_sink.set_on_audio_state_changed([](esp_a2d_audio_state_t audioState, void *ptr) {
    if (audioState == ESP_A2D_AUDIO_STATE_STARTED) {
      state = bluetoothPlaying;
    } else if (audioState == ESP_A2D_AUDIO_STATE_STOPPED || audioState == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND) {
      state = bluetoothPaused;
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
  return hexstr;
}

void audio_eof_mp3(const char *info) {  // end of file
  checkBattery(!player.currentlyPlayingFile.equals(info));
  player.next();
  if (fileWhenTagInserted == player.currentlyPlayingFile) {
    // shutdown when starting file is reached again
    fileWhenTagInserted = "";
    shutdown();
  }
}

void setVolume() {
  player.audio.setVolume(::map(analogRead(VOLUME_PIN), 0, 4095, 2, maxVolume));
}

bool isTokenPresent() {
  int batteryPercent = getBattery();
  return batteryPercent > 0;
}

void handleVolumeBoost() {
  if (digitalRead(VOLUME_BOOST_PIN) == LOW &&
      millis() - maxVolumeBoostLastRead > maxVolumeBoostTimeout) {
    maxVolumeBoostLastRead = millis();
    maxVolumeIsSafe = !maxVolumeIsSafe;
    // maxVolume = (maxVolume == MAX_VOLUME_LOUD)
    // ? MAX_VOLUME_SAFE
    // : MAX_VOLUME_LOUD;
    Serial.print("Volume boost set to ");
    Serial.println(maxVolumeIsSafe ? "SAFE" : "LOUD");
  }

  maxVolume = maxVolumeIsSafe ? player.songMaxVolume > 0 &&
                                        player.songMaxVolume <= MAX_VOLUME_LOUD
                                    ? player.songMaxVolume
                                    : MAX_VOLUME_SAFE
                              : MAX_VOLUME_LOUD;
}

void checkBattery(bool playAlert) {
  int batteryPercent = getBattery();
  Serial.println("Battery level is " + String(batteryPercent) + "%");
  if (playAlert) {
    if (batteryPercent > LOW_BATTERY_THRESHOLD) {
    }
  } else {
    if (batteryPercent <= LOW_BATTERY_THRESHOLD) {
      player.playAlert();
    }
  }
}

unsigned int getBattery() {
  uint16_t acc = 0;
  for (size_t i = 0; i < 10; i++) {
    acc += analogRead(VOLTAGE_PIN);
  }
  unsigned int value = acc / 10;
  // unsigned int batteryPercent = ::map(value, 400, 3100, 0, 100);
  unsigned int batteryPercent = ::max(0L, ::min(100L, ::map(value, 2074, 2803, 0, 100)));
  return batteryPercent;
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
  } else {
    Serial.println("Command \"" + cmd +
                   "\" is not supported. Supported commands: low[:<-40..6>], "
                   "mid[:<-40..6>], high[:<-40..6>], list, play:<index>, "
                   "maxVolume:<safe|unsafe>");
  }
  Serial.readString();
}

void restoreToneSettings() {
  player.audio.setTone(settings.getInt("low", 0), settings.getInt("mid", 0),
                       settings.getInt("high", 0));
}

void shutdown() {
  Serial.println("shutting down");
  digitalWrite(POWER_SUPPLY_PIN, LOW);
}