// ============================================================
// AUTONOMOUS MOTOR SPIN - clockwise, no PC/USB required
// Build/upload with: pio run -e motor_autospin -t upload
//
// Starts automatically the instant power is applied (battery
// only, no USB needed after flashing). Spins clockwise
// continuously - no stopping, no reversing, no commands needed.
// ============================================================
#include <Arduino.h>

const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;

const int SPIN_SPEED = 200;

void setup() {
  Serial.begin(9600); // optional - runs fine with no USB connected

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable driver
  digitalWrite(MOTOR_DIR_PIN, HIGH);   // clockwise
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED);

  Serial.println("=== Autonomous Motor Spin - clockwise, running on battery power ===");
}

void loop() {
  // nothing to do - motor just keeps spinning
}
