// ============================================================
// LASER TEST - Nano V3
// Build/upload with: pio run -e laser_test -t upload
// Then:              pio device monitor -e laser_test
//
// NOTE: pin is 6, NOT 8. Pin 8 is used for the motor's DIR
// signal in motor_onoff and connection_test - using it for the
// laser too would conflict once subsystems are combined.
//
// Behaviour: blinks the laser ON 1s / OFF 1s, with status printed
// to Serial so you can confirm it's actually running even before
// you can see the beam clearly.
// ============================================================
#include <Arduino.h>

const int LASER_PIN = 6;

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW); // start OFF - don't power up already firing

  Serial.println("=== Laser Test (Nano V3) ===");
  Serial.println("Blinking ON 1s / OFF 1s.");
  Serial.println("Reminder: this is the eye-safe module (Jaycar XC4490) but");
  Serial.println("still avoid staring directly into the beam or pointing at eyes.");
}

void loop() {
  Serial.println("Laser ON");
  digitalWrite(LASER_PIN, HIGH);
  delay(1000);

  Serial.println("Laser OFF");
  digitalWrite(LASER_PIN, LOW);
  delay(1000);
}
