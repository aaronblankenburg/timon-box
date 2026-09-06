#ifndef KNOCK_CLASS
#define KNOCK_CLASS

#include "Arduino.h"

class KnockSensor {
 private:
  enum State {
    IDLE,      // wartet auf Knock-Beginn
    KNOCKING,  // sammelt Werte während Knock
    COOLDOWN   // kurze Pause nach Knock
  };

  byte pin;
  unsigned long threshold;
  const float alpha = 0.005;

  unsigned long maxKnockLengthMs = 200;
  unsigned long timeBeforeReKnock = 500;

  unsigned long knockStartTime = 0;
  unsigned long accumulator = 0;
  unsigned long loopCounter = 0;
  unsigned int value = 0;
  bool actionPending = true;

  State state = IDLE;

 public:
  void (*action)() = []() { Serial.println("knock"); };

  KnockSensor(byte pin, unsigned long threshold)
      : pin(pin), threshold(threshold) {
    pinMode(pin, INPUT);
  }

  void loop() {
    {
      // Gleitender Mittelwert für Signalberuhigung
      int raw = analogRead(pin);
      value = alpha * raw + (1 - alpha) * value;

      unsigned long now = millis();
      unsigned long elapsed = now - knockStartTime;

      switch (state) {
        case IDLE:
          // Warten auf neuen Knock
          if (value > 0) {
            state = KNOCKING;
            knockStartTime = now;
            accumulator = 0;
            loopCounter = 0;
            actionPending = true;
            // Serial.println("State: KNOCKING");
          }
          break;

        case KNOCKING: {
          accumulator += value;
          loopCounter++;

          unsigned long area = accumulator * elapsed / max(1UL, loopCounter);
          if (area > threshold && actionPending) {
            action();
            actionPending = false;
            Serial.print("Press detected, area: ");
            Serial.print(area);
            Serial.print(", duration: ");
            Serial.println(elapsed);
            state = COOLDOWN;
          }

          if (elapsed > maxKnockLengthMs) {
            state = COOLDOWN;
            // Serial.println("State: COOLDOWN");
          }
          break;
        }
        case COOLDOWN:
          //   if (accumulator > 0) {
          //     Serial.print("area: ");
          //     Serial.println(accumulator * elapsed / max((unsigned long)1, loopCounter));
          //   }

          if (elapsed > maxKnockLengthMs + timeBeforeReKnock) {
            accumulator = 0;
            loopCounter = 0;
            actionPending = true;
            state = IDLE;
            // Serial.println("State: IDLE");
          }
          break;
      }
    }
  }
};

#endif
