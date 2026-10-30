// Per-box settings. Every box sets its own values in its env in platformio.ini (-D NAME=value); the values here
// are only the fallback. How to find them out: doc/hardware.md, "Inbetriebnahme". Only #defines in this file.
#ifndef BOX_CONFIG_H
#define BOX_CONFIG_H

// Special tokens: the UID exactly as the serial console prints it when the token is placed ("tag: <uid>").
#ifndef BLUETOOTH_TAG
#define BLUETOOTH_TAG "04c6d4b26e6780"
#endif
#ifndef MANAGEMENT_SERVER_TAG
#define MANAGEMENT_SERVER_TAG "042041122a6781"
#endif

// Battery display: two calibration points, each the raw ADC value at the Sense pin (serial command "battery" or the
// management page) and Vcc in mV measured with a multimeter at the same moment. Linear in between and beyond.
#ifndef BATTERY_ADC_LOW
#define BATTERY_ADC_LOW 1994
#endif
#ifndef BATTERY_MV_LOW
#define BATTERY_MV_LOW 3685
#endif
#ifndef BATTERY_ADC_HIGH
#define BATTERY_ADC_HIGH 2390
#endif
#ifndef BATTERY_MV_HIGH
#define BATTERY_MV_HIGH 4322
#endif

#endif
