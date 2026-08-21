// ============================================================
// LASER TEST
// Build/upload with: pio run -e laser_test -t upload
//
// NOTE: pin changed from 8 to 6. Pin 8 is used for the motor's
// DIR signal in your other test sketches (motor_onoff,
// motor_sequence, motor_flag_sequence, motor_autonomous) - using
// it for the laser too would conflict once you combine subsystems.
// Pin 6 matches LASER_PIN already reserved in include/Config.h.
//
// Behaviour: blinks the laser ON 1s / OFF 1s, same timing as your
// original, with status printed to Serial so you can confirm it's
// actually running even before you can see the beam clearly.
// ============================================================
#include <Arduino.h>

const int LASER_PIN = 8; // was 8 in your original - see note above

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW); // start OFF - don't power up already firing

  Serial.println("=== Laser Test ===");
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
