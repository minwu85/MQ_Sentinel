// ============================================================
// MOTOR ON/OFF SPIN TEST - PlatformIO / VS Code version
// Build/upload with: pio run -e motor_onoff -t upload
//
// Purpose: simplest possible test - just turn the motor fully
// ON for a few seconds, then fully OFF, and repeat. No encoder,
// no direction switching, no PWM ramping - just confirm the
// DRV8874 driver + battery pack + motor wiring actually works.
// ============================================================
#include <Arduino.h>

// --- Pin definitions (match your DRV8874 wiring) ---
const int MOTOR_PWM_PIN   = 9;   // driver PWM (speed)
const int MOTOR_DIR_PIN   = 8;   // driver DIR (direction)
const int MOTOR_SLEEP_PIN = 7;   // driver nSLEEP - must be HIGH to enable

void setup() {
  Serial.begin(9600);
  Serial.println("=== Motor ON/OFF Test ===");

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // wake the driver - REQUIRED or nothing spins
  digitalWrite(MOTOR_DIR_PIN, HIGH);   // pick one direction, doesn't matter for this test
  analogWrite(MOTOR_PWM_PIN, 0);       // start OFF

  Serial.println("Setup complete. Starting ON/OFF loop in 2 seconds...");
  delay(2000);
}

void loop() {
  Serial.println("Motor ON (full speed)");
  analogWrite(MOTOR_PWM_PIN, 255);   // full speed ON
  delay(3000);

  Serial.println("Motor OFF");
  analogWrite(MOTOR_PWM_PIN, 0);     // OFF
  delay(2000);
}
