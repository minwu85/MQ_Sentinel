// ============================================================
// MOTOR SPIN TEST - stop with spacebar, verbose terminal output
// Build/upload with: pio run -e motor_onoff -t upload
//
// Behaviour:
//   - Send 'g' (then Enter) to start spinning continuously
//   - Send ' ' (space, then Enter) to stop
//   - Prints live status (running/stopped, elapsed time) every
//     second while spinning, so the terminal always tells you
//     exactly what state the motor is in
// ============================================================
#include <Arduino.h>

const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;

const int SPIN_SPEED = 255; // full speed

bool motorRunning = false;
unsigned long spinStartMs = 0;
unsigned long lastStatusPrintMs = 0;

void startMotor() {
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED);
  motorRunning = true;
  spinStartMs = millis();
  Serial.println(">>> MOTOR STARTED <<<");
}

void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  motorRunning = false;
  Serial.println(">>> MOTOR STOPPED <<<");
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // wake the driver - REQUIRED or nothing spins
  analogWrite(MOTOR_PWM_PIN, 0);       // start OFF

  Serial.println("=== Motor Spin Test (space to stop) ===");
  Serial.println("Send 'g' to start spinning, send ' ' (space) to stop.");
}

void loop() {
  // --- Handle incoming commands without blocking ---
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g' || c == 'G') {
      if (!motorRunning) startMotor();
    } else if (c == ' ') {
      if (motorRunning) stopMotor();
    }
  }

  // --- Print live status once per second while running ---
  if (motorRunning && millis() - lastStatusPrintMs >= 1000) {
    lastStatusPrintMs = millis();
    unsigned long elapsedSec = (millis() - spinStartMs) / 1000;
    Serial.print("Status: RUNNING | speed=");
    Serial.print(SPIN_SPEED);
    Serial.print(" | elapsed=");
    Serial.print(elapsedSec);
    Serial.println("s | send ' ' to stop");
  }
}
