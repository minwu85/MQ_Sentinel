#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------- IR Sensor Ring ----------
// Number of IR receivers mounted around the sensor ring
#define NUM_IR_SENSORS 3

// Digital pins for each IR receiver (demodulated output, e.g. TSOP-style module)
// Fill these in once your sensor ring wiring is finalised
static const uint8_t IR_SENSOR_PINS[NUM_IR_SENSORS] = {2, 3, 4};

// ---------- Motor / Reaction Wheel ----------
// Pololu DRV8874 motor driver control pins
#define MOTOR_PWM_PIN   9   // speed control (PWM)
#define MOTOR_DIR_PIN   8   // direction control
#define MOTOR_SLEEP_PIN 7   // nSLEEP - HIGH to enable driver

// Encoder pins (FIT0186 incremental encoder)
#define ENCODER_A_PIN 2
#define ENCODER_B_PIN 3

// ---------- Laser ----------
#define LASER_PIN 6   // eye-safe laser diode module control pin

// ---------- Mock beacon button (software-only testing, no IR hardware needed) ----------
#define MOCK_BEACON_BUTTON_PIN 5   // pushbutton to GND, uses internal pullup

// ---------- Timing ----------
#define LASER_DWELL_MS 2000UL      // must hold laser on target for 2 seconds
#define DEMO_DURATION_MS (5UL * 60UL * 1000UL) // 5 minute demonstration

// ---------- IR modulation ----------
#define IR_CARRIER_FREQ_HZ 38000UL

#endif
