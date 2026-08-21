#include <Arduino.h>

const int PIN_SLEEP = 7;
const int PIN_DIR   = 8;
const int PIN_PWM   = 9;

bool hasRun = false; // Tracks if the 4-step sequence is complete

void setup() {
  digitalWrite(PIN_SLEEP, HIGH); 

  pinMode(PIN_SLEEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_PWM, OUTPUT);
}

void loop() {
  // Only run if it hasn't completed a full cycle yet
  if (!hasRun) {
    // 1. Forward (5 seconds)
    digitalWrite(PIN_DIR, HIGH);
    analogWrite(PIN_PWM, 128);
    delay(5000);

    // 2. Stop (2 seconds)
    analogWrite(PIN_PWM, 0);
    delay(2000);

    // 3. Reverse (5 seconds)
    digitalWrite(PIN_DIR, LOW);
    analogWrite(PIN_PWM, 128);
    delay(5000);

    // 4. Stop (2 seconds)
    analogWrite(PIN_PWM, 0);
    delay(2000);

    hasRun = true; // Set flag so loop() stops executing this block
  }
}