#ifndef KNOCK_CLASS
#define KNOCK_CLASS

#include "Arduino.h"

class KnockSensor {
    private:
    byte pin;
    unsigned long threshold;
    bool actionPending = true;
    unsigned long maxKnockLengthMs = 200;
    unsigned long timeBeforeReKnock = 500;
    unsigned long knockStartTime = 0;
    unsigned long accumulator = 0;
    unsigned long loopCounter = 0;

public:
    void (*action)() = [](){Serial.println("knock");};

    KnockSensor(byte pin, unsigned long threshold): pin(pin), threshold(threshold) {
        pinMode(pin, INPUT);
    }

    void loop(){
        unsigned int value = analogRead(pin);

        unsigned long timeSinceKnockStart = millis()-knockStartTime;
        bool isInAKnock = timeSinceKnockStart < maxKnockLengthMs;
        bool isSensing = timeSinceKnockStart > (maxKnockLengthMs + timeBeforeReKnock);

        if(isInAKnock) {
            accumulator += value;
            loopCounter++;

            unsigned long area = accumulator * timeSinceKnockStart / loopCounter;
            if(area > threshold && actionPending) {
                action();
                actionPending = false;
            }
        } else if(isSensing) {
            if(accumulator > 0 ) Serial.println("area: " + String(accumulator * timeSinceKnockStart / loopCounter));
            accumulator = 0;
            actionPending = true;

            if(value > 0) {
                knockStartTime = millis();
                loopCounter = 0;
            }
        }

    }

};

#endif