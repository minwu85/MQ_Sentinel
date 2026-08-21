// ============================================================
// MOTOR SEQUENCE TEST - non-blocking "hasRun" flag version
// Build/upload with: pio run -e motor_flag_sequence -t upload
//
// Runs ONE full forward/stop/reverse/stop cycle, then halts.
// Bug fixed from original: pinMode() must be called BEFORE
// digitalWrite() on that pin, otherwise the pin isn't guaranteed
// to be configured as an output yet.
// ============================================================
#include <Arduino.h>

const int PIN_SLEEP = 7;
const int PIN_DIR   = 8;
const int PIN_PWM   = 9;

bool hasRun = false; // Tracks if the 4-step sequence is complete

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(PIN_SLEEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_PWM, OUTPUT);

  digitalWrite(PIN_SLEEP, HIGH); // now safe - pin is configured as OUTPUT first

  Serial.println("=== Motor Flag Sequence Test ===");
  Serial.println("Running once: forward 5s -> stop 2s -> reverse 5s -> stop 2s");
}

void loop() {
  // Only run if it hasn't completed a full cycle yet
  if (!hasRun) {
    // 1. Forward (5 seconds)
    Serial.println("Forward - 5 seconds");
    digitalWrite(PIN_DIR, HIGH);
    analogWrite(PIN_PWM, 128);
    delay(5000);

    // 2. Stop (2 seconds)
    Serial.println("Stop - 2 seconds");
    analogWrite(PIN_PWM, 0);
    delay(2000);

    // 3. Reverse (5 seconds)
    Serial.println("Reverse - 5 seconds");
    digitalWrite(PIN_DIR, LOW);
    analogWrite(PIN_PWM, 128);
    delay(5000);

    // 4. Stop (2 seconds)
    Serial.println("Stop - 2 seconds");
    analogWrite(PIN_PWM, 0);
    delay(2000);

    hasRun = true; // Set flag so loop() stops executing this block
    Serial.println("Sequence complete. (Re-upload to run again.)");
  }
}
