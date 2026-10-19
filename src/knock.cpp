#ifndef KNOCK_CLASS
#define KNOCK_CLASS

#include "Arduino.h"
#include "driver/adc.h"
#include "esp_timer.h"

// Piezo disc glued to a flexible panel, 100k and a 3.3 V Zener in parallel, read by the ADC.
//
// Bending the disc drives a current through the resistor, so while the finger is moving the ADC
// sees one positive hump: its height follows the speed of the movement, its area the distance.
// Sound from the speaker shakes the panel too, but as an oscillation. The ADC only sees the
// positive half waves, so a tone is a train of short humps with gaps at zero in between, each
// no longer than half a period - under 9 ms even for the lowest trombone note (58 Hz).
//
// A press is therefore told apart from music by the shape of one uninterrupted hump, not by
// the level of a filtered signal (a rectified tone has a mean level as well, which is why
// loud notes used to skip tracks). A hump counts as a press when it
//   - lasts at least minPressMs without dropping to zero (gentle, slow press), or
//   - integrates to areaThreshold counts*ms (quick or firm press; the area only depends on
//     how far the panel was pushed, not on how fast).
//
// Seeing the gaps needs samples much closer together than a half wave. The main loop only gets
// round every ~9 ms while audio.loop() waits for the I2S buffer, so the signal is sampled by an
// esp_timer at 2 kHz instead (core 0, where nothing else runs during playback). The timer only
// runs the state machine; loop() fires the action and prints from the main task.

// Tuning per box, overridable in platformio.ini (-D KNOCK_ON_LEVEL=200 ...): a thin, lively panel gives
// bigger signals for presses and music alike and needs higher levels and areas. Lower onLevel / higher
// maxGapUs / lower minPressMs = more sensitive to slow, gentle presses. The limits: maxGapUs must stay
// below the half-wave gap of the highest tone that shakes the panel (1 ms at 500 Hz), minPressMs above
// the half wave of the lowest (8.6 ms at 58 Hz). Below ~0.1 V the ADC reads 0, no constant helps there -
// a bigger resistor across the piezo does (see README).
#ifndef KNOCK_ON_LEVEL
#define KNOCK_ON_LEVEL 50  // counts: a hump starts when the signal rises above this ...
#endif
#ifndef KNOCK_OFF_LEVEL
#define KNOCK_OFF_LEVEL (KNOCK_ON_LEVEL * 3 / 5)  // ... and ends once it has been below this for maxGapUs
#endif
#ifndef KNOCK_MAX_GAP_US
#define KNOCK_MAX_GAP_US 700  // with 500 us between samples: 300 = one low sample ends the hump, 700 = two do
#endif
#ifndef KNOCK_MIN_PRESS_MS
#define KNOCK_MIN_PRESS_MS 15  // an uninterrupted hump this long is a press: no note has a half wave this long
#endif

class KnockSensor {
 private:
  enum State {
    IDLE,     // waiting for the signal to rise
    HUMP,     // inside a hump, integrating it
    COOLDOWN  // fired; ignore the panel while it is released and the click plays
  };

  byte pin;
  int8_t channel = -1;          // ADC1 channel of the pin, read directly by the sampler
  unsigned long areaThreshold;  // counts*ms; per sensor, the panels differ mechanically

  static constexpr unsigned int onLevel = KNOCK_ON_LEVEL;
  static constexpr unsigned int offLevel = KNOCK_OFF_LEVEL;
  static constexpr unsigned long maxGapUs = KNOCK_MAX_GAP_US;
  static constexpr unsigned long minPressMs = KNOCK_MIN_PRESS_MS;
  static constexpr unsigned long sampleUs = 500;     // 2 kHz: several samples in the 1 ms gap of a 500 Hz tone
  static constexpr unsigned long maxRiseUs = 10000;  // a hump is timed from when the signal left rest, but no
                                                     // further back than this (a raised noise floor is not a hump)
  static constexpr unsigned long maxDtUs = 2000;     // a late timer callback must not inflate the area
  static constexpr unsigned long cooldownMs = 800;   // after firing: the panel is released and the click plays
  static constexpr unsigned long maxPressAgeUs = 200000;  // a press seen while paused is not replayed when playback starts

  // --- sampler state, touched only from the timer callback ---
  State state = IDLE;
  bool armed = false;  // the signal has been at rest since the last press: a stuck signal fires once, not every cooldown
  unsigned long lastSampleUs = 0;
  unsigned long restUs = 0;       // last idle sample below offLevel
  unsigned long lastAboveUs = 0;  // last sample of the current hump at or above offLevel
  unsigned long humpStartUs = 0;
  unsigned long firedUs = 0;
  unsigned long area = 0;  // counts*us of the current hump
  unsigned int peak = 0;

  // --- handed from the sampler to loop() ---
  volatile unsigned long pressedUs = 0;  // when the last press fired; loop() resets it after acting
  unsigned long pressedMs = 0, pressedArea = 0;
  unsigned int pressedPeak = 0;

  // What the music does to the sensor: per-second statistics of the humps that did not fire.
  unsigned long statsSinceMs = 0;
  unsigned long samples = 0, readUs = 0;
  unsigned int humps = 0, longestMs = 0, maxPeak = 0;
  unsigned long maxArea = 0;

  // One-shot trace of the raw signal (debug = 2): starts when the signal leaves rest, printed when full.
  static constexpr unsigned int traceLen = 400;
  static constexpr unsigned long traceUs = 300000;
  volatile bool traceReady = false;
  unsigned long traceStartUs = 0;
  unsigned int traceCount = 0;
  unsigned long traceT[traceLen];
  unsigned int traceRaw[traceLen];

  esp_timer_handle_t timer = nullptr;

  void noteHump(unsigned long durationMs) {
    humps++;
    if (durationMs > longestMs) longestMs = durationMs;
    if (peak > maxPeak) maxPeak = peak;
    if (area / 1000 > maxArea) maxArea = area / 1000;
  }

  void trace(unsigned long now, unsigned int raw) {
    if (debug < 2 || traceReady) return;
    if (traceCount == 0) {
      if (raw < offLevel) return;
      traceStartUs = now;
    }
    traceT[traceCount] = now - traceStartUs;
    traceRaw[traceCount] = raw;
    traceCount++;
    if (traceCount == traceLen || now - traceStartUs >= traceUs) traceReady = true;
  }

  void sample() {
    unsigned long now = micros();
    unsigned int raw = adc1_get_raw((adc1_channel_t)channel);
    readUs += micros() - now;
    unsigned long dt = now - lastSampleUs;
    if (dt > maxDtUs) dt = maxDtUs;
    lastSampleUs = now;
    samples++;
    trace(now, raw);

    switch (state) {
      case COOLDOWN:
        if (now - firedUs >= cooldownMs * 1000) {
          state = IDLE;
          armed = false;
        }
        break;

      case IDLE:
        if (raw < offLevel) {
          armed = true;
          restUs = now;
        } else if (armed && raw >= onLevel) {
          state = HUMP;
          humpStartUs = now - restUs < maxRiseUs ? restUs : now - maxRiseUs;
          lastAboveUs = now;
          area = raw * dt;
          peak = raw;
        }
        break;

      case HUMP: {
        if (raw >= offLevel) {
          lastAboveUs = now;
        } else if (now - lastAboveUs > maxGapUs) {
          // dropped to zero: a half wave of a tone, or a touch too small to count
          noteHump((now - humpStartUs) / 1000);
          state = IDLE;
          armed = true;
          restUs = now;
          break;
        }
        area += raw * dt;
        if (raw > peak) peak = raw;

        unsigned long durationMs = (now - humpStartUs) / 1000;
        if (durationMs >= minPressMs || area / 1000 >= areaThreshold) {
          pressedMs = durationMs;
          pressedPeak = peak;
          pressedArea = area / 1000;
          firedUs = now;
          pressedUs = now;
          state = COOLDOWN;
        }
        break;
      }
    }
  }

  void printStats() {
    if (!debug || millis() - statsSinceMs < 1000) return;
    Serial.printf("knock %u: %lu samples/s (%lu us each), %u humps, longest %u ms, peak %u, area %lu\n", pin, samples,
                  samples ? readUs / samples : 0, humps, longestMs, maxPeak, maxArea);
    statsSinceMs = millis();
    samples = readUs = 0;
    humps = longestMs = maxPeak = 0;
    maxArea = 0;
  }

  void printTrace() {
    if (!traceReady) return;
    Serial.printf("knock %u: trace, %u samples as us:counts\n", pin, traceCount);
    for (unsigned int i = 0; i < traceCount; i++) {
      Serial.printf("%lu:%u%c", traceT[i], traceRaw[i], (i % 10 == 9 || i == traceCount - 1) ? '\n' : ' ');
    }
    traceCount = 0;
    debug = 1;
    traceReady = false;
  }

 public:
  volatile uint8_t debug = 0;  // serial command "knockdebug:<0|1|2>": off, per-second statistics, plus one raw trace
  void (*action)() = []() { Serial.println("knock"); };

  KnockSensor(byte pin, unsigned long areaThreshold) : pin(pin), areaThreshold(areaThreshold) {
    pinMode(pin, INPUT);
  }

  // Start sampling. Call once from setup().
  void begin() {
    // Keep the SAR ADC powered instead of letting every read switch it on and off: the toggling at
    // kHz rates disturbs the WiFi PHY (see updateBatteryAdc()).
    adc_power_acquire();
    // One analogRead() lets the Arduino core set up the ADC and attach the pin. It repeats that pin
    // setup on every call (~100 us), so the sampler reads the channel directly afterwards.
    analogRead(pin);
    channel = digitalPinToAnalogChannel(pin);
    if (channel < 0 || channel >= SOC_ADC_MAX_CHANNEL_NUM) {
      Serial.printf("knock %u: not an ADC1 pin, sensor disabled\n", pin);
      return;
    }
    esp_timer_create_args_t args = {};
    args.callback = [](void *self) { static_cast<KnockSensor *>(self)->sample(); };
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "knock";
    esp_timer_create(&args, &timer);
    esp_timer_start_periodic(timer, sampleUs);
  }

  // Runs the action for a press the sampler detected. Call from the main loop while presses matter.
  void loop() {
    if (pressedUs) {
      unsigned long age = micros() - pressedUs;
      pressedUs = 0;
      Serial.printf("knock %u: press after %lu ms, peak %u, area %lu\n", pin, pressedMs, pressedPeak, pressedArea);
      if (age < maxPressAgeUs) action();
    }
    printStats();
    printTrace();
  }
};

#endif
