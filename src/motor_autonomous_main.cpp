// ============================================================
// AUTONOMOUS MOTOR TEST - no PC/USB required to run
// Build/upload with: pio run -e motor_autonomous -t upload
//
// Behaviour: starts automatically the instant power is applied
// (battery only, no USB needed after flashing). Loops forever:
//   forward 5s -> stop 2s -> reverse 5s -> stop 2s -> repeat
//
// Serial prints are still included - if you DO plug in USB later
// to check on it, you'll see status. But the sketch does not wait
// for or require any serial connection or input to run.
// ============================================================
#include <Arduino.h>

const int PIN_SLEEP = 7;
const int PIN_DIR   = 8;
const int PIN_PWM   = 9;

const int SPIN_SPEED = 200;
const unsigned long SPIN_DURATION_MS = 5000UL;
const unsigned long PAUSE_MS = 2000UL;

void setup() {
  // Serial is optional here - if nothing is listening (no USB), these
  // calls are harmless and the sketch continues normally regardless.
  Serial.begin(9600);

  pinMode(PIN_SLEEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_PWM, OUTPUT);

  digitalWrite(PIN_SLEEP, HIGH); // enable driver
  analogWrite(PIN_PWM, 0);

  Serial.println("=== Autonomous Motor Test - running on battery power ===");
  Serial.println("No PC connection required - this runs standalone.");
}

void loop() {
  Serial.println("Forward - 5 seconds");
  digitalWrite(PIN_DIR, HIGH);
  analogWrite(PIN_PWM, SPIN_SPEED);
  delay(SPIN_DURATION_MS);

  Serial.println("Stop - 2 seconds");
  analogWrite(PIN_PWM, 0);
  delay(PAUSE_MS);

  Serial.println("Reverse - 5 seconds");
  digitalWrite(PIN_DIR, LOW);
  analogWrite(PIN_PWM, SPIN_SPEED);
  delay(SPIN_DURATION_MS);

  Serial.println("Stop - 2 seconds");
  analogWrite(PIN_PWM, 0);
  delay(PAUSE_MS);

  // no hasRun flag - loops forever automatically, no command needed
}
