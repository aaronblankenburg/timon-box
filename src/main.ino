// will play many audio file formats, mp3,aac,flac etc.
// See github page : https://github.com/schreibfaul1/ESP32-audioI2S
#include <Adafruit_NeoPixel.h>
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
#define SWITCH_TOKEN_PRESENT GPIO_NUM_32
#define VOLTAGE_PIN GPIO_NUM_39
#define NEO_PIXEL_PIN GPIO_NUM_2

#define MAX_VOLUME_LOUD 21
#define MAX_VOLUME_SAFE 17
#define LOW_BATTERY_THRESHOLD 20  // in %
#define MAX_PAUSE_TIME_IN_S 10 * 60
#define MAX_PLAY_TIME_IN_S 120 * 60
#define LEFT_AND_RIGHT_SIMULTANOUS_PRESS_TIMEOUT_IN_MS 750

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
             managementServerWaitForTag,
             managementServerRunning };
const uint32_t stateColors[] = {
    /* paused */ pixel.Color(255, 165, 0),                      // Orange
    /* playing */ pixel.Color(0, 255, 0),                       // Green
    /* bluetoothPlaying */ pixel.Color(0, 0, 255),              // Blue
    /* bluetoothPaused */ pixel.Color(75, 0, 130),              // Indigo
    /* managementServerWaitForTag */ pixel.Color(255, 255, 0),  // Yellow
    /* managementServerRunning */ pixel.Color(255, 165, 0)      // Orange (updated)
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
  settings.begin("settings", false);
  restoreToneSettings();

  pinMode(VOLUME_PIN, INPUT);
  pinMode(SWITCH_TOKEN_PRESENT, INPUT_PULLUP);
  pinMode(VOLTAGE_PIN, INPUT);

  if (esp_reset_reason() != ESP_RST_SW) bluetoothMode = false;
  if (bluetoothMode) {
    startBluetooth();
  } else {
    startPlayer();
  }
  SPI.begin(CR_SCK, CR_MISO, CR_MOSI, CR_CS);
  rfid.PCD_Init(CR_CS, CR_RST);
  rfid.PCD_SetAntennaGain(rfid.RxGain_23dB);
  // Erhöhe die ASK‑Modulation auf 100 %:
  // rfid.PCD_WriteRegister(MFRC522::TxASKReg, B01000000);
  // Optional: niedrigere Empfangsschwelle
  // rfid.PCD_WriteRegister(MFRC522::RxThresholdReg, B00000000);
  // rfid.PCD_WriteRegister(MFRC522::RFCfgReg, B01110000);
  batteryAdcValueManagementServer = &batteryAdcValue;
}

void loop() {
  esp_task_wdt_reset();
  checkSerialForCommands();
  updateBatteryAdc();

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
        changeState(bluetoothPaused);
        break;
      }

      if (recent_tag == MANAGEMENT_SERVER_TAG) {
        Serial.println("management server mode, waiting for token");
        changeState(managementServerWaitForTag);
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
        changeState(playing);
      }

      if (millis() - musicStopTime > (MAX_PAUSE_TIME_IN_S * 1000)) {
        shutdownBox();
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
        changeState(paused);
        delay(3000);
      }

      if (millis() - musicStartTime > (MAX_PLAY_TIME_IN_S * 1000)) {
        player.pause();
        player.savePlayingState();
        shutdownBox();
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

    case managementServerWaitForTag:
      if (recent_tag != "" && recent_tag != MANAGEMENT_SERVER_TAG) {
        player.setupManagementServer(recent_tag);
        changeState(managementServerRunning);
      }
      break;

    case managementServerRunning:
      if (recent_tag == "") {
        shutdownBox();
        changeState(paused);
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

void changeState(State newState) {
  if (state == newState) return;

  state = newState;
  if (getBattery() > LOW_BATTERY_THRESHOLD) {
    setNeoPixel(stateColors[state]);
  } else {
    setNeoPixel(pixel.Color(255, 0, 0));
  }
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

void audio_eof_mp3(const char *info) {  // end of file
  checkBattery(!player.currentlyPlayingFile.equals(info));
  player.next();
  if (fileWhenTagInserted == player.currentlyPlayingFile) {
    // shutdown when starting file is reached again
    fileWhenTagInserted = "";
    shutdownBox();
  }
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
  if (playAlert) {
    if (batteryPercent <= LOW_BATTERY_THRESHOLD) {
      player.playAlert();
    }
  } else {
    if (batteryPercent > LOW_BATTERY_THRESHOLD) {
    }
  }
  setNeoPixel(pixel.Color(255, 0, 0));
}

unsigned int getBattery() {
  // unsigned int batteryPercent = ::map(value, 400, 3100, 0, 100);
  unsigned int batteryPercent = constrain(::map(::map((uint)batteryAdcValue, 1994, 2390, 3685, 4322), 3200, 4200, 0, 100), 0, 100);
  return batteryPercent;
}

void updateBatteryAdc() {
  batteryAdcValue = (batteryAdcValue * 0.999) + (0.001 * analogRead(VOLTAGE_PIN));
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

void shutdownBox() {
  Serial.println("shutting down");
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));  // Set to off (black)
  digitalWrite(POWER_SUPPLY_PIN, LOW);
}