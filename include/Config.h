#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------- IR Sensor Ring ----------
// Number of IR receivers mounted around the sensor ring
#define NUM_IR_SENSORS 3

// Digital pins for each IR receiver (demodulated output, e.g. TSOP-style module)
// Matches the left/right/centre convention used elsewhere in the project
static const uint8_t IR_SENSOR_PINS[NUM_IR_SENSORS] = {4, 5, 6}; // left, right, centre

#endif
