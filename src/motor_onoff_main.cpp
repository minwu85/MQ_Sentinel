// ============================================================
// MOTOR ON/OFF + DIRECTION - Nano V3
// Build/upload with: pio run -e motor_onoff -t upload
// Then:              pio device monitor -e motor_onoff
//
// Commands (send via Serial, then Enter):
//   g  - start spinning continuously
//   ' ' (space) - stop
//   r  - reverse direction - switches to continuous spin in the
//        OTHER direction. If the motor was stopped, 'r' also
//        starts it spinning in the new direction. Press 'r' again
//        to flip back, and it keeps spinning continuously each time.
// ============================================================
#include <Arduino.h>

const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;

const int SPIN_SPEED = 255;

bool motorRunning = false;
bool directionForward = true; // true = HIGH/clockwise, false = LOW/anti-clockwise
unsigned long spinStartMs = 0;
unsigned long lastStatusPrintMs = 0;

void applyDirection() {
  digitalWrite(MOTOR_DIR_PIN, directionForward ? HIGH : LOW);
}

void startMotor() {
  applyDirection();
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED);
  motorRunning = true;
  spinStartMs = millis();
  Serial.print(">>> MOTOR STARTED (");
  Serial.print(directionForward ? "clockwise" : "anti-clockwise");
  Serial.println(") <<<");
}

void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  motorRunning = false;
  Serial.println(">>> MOTOR STOPPED <<<");
}

void reverseAndKeepSpinning() {
  directionForward = !directionForward;
  applyDirection();
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED); // (re)apply speed so it spins continuously either way
  motorRunning = true;
  spinStartMs = millis();
  Serial.print(">>> DIRECTION REVERSED - now spinning continuously (");
  Serial.print(directionForward ? "clockwise" : "anti-clockwise");
  Serial.println(") <<<");
}

void setup() {
  Serial.begin(9600); // optional - harmless if no USB is connected
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // wake the driver - REQUIRED or nothing spins
  applyDirection();

  Serial.println("=== Motor On/Off + Direction (Nano V3) ===");
  Serial.println("g = start | space = stop | r = reverse + keep spinning continuously");
  Serial.println("Auto-starting spin now (works with or without USB connected)...");

  // Auto-start immediately - this means the motor begins spinning the
  // instant power is applied (battery on VIN, no USB needed), rather
  // than waiting idle for a 'g' command that can only arrive over USB.
  // If USB IS connected, you can still send g/space/r as normal.
  startMotor();
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g' || c == 'G') {
      if (!motorRunning) startMotor();
    } else if (c == ' ') {
      if (motorRunning) stopMotor();
    } else if (c == 'r' || c == 'R') {
      reverseAndKeepSpinning();
    }
  }

  if (motorRunning && millis() - lastStatusPrintMs >= 1000) {
    lastStatusPrintMs = millis();
    unsigned long elapsedSec = (millis() - spinStartMs) / 1000;
    Serial.print("Status: RUNNING | dir=");
    Serial.print(directionForward ? "CW" : "CCW");
    Serial.print(" | speed=");
    Serial.print(SPIN_SPEED);
    Serial.print(" | elapsed=");
    Serial.print(elapsedSec);
    Serial.println("s");
  }
}
