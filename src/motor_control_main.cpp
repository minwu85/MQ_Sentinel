// ============================================================
// MOTOR CONTROL - g = start, space = stop, r = reverse direction
// Build/upload with: pio run -e motor_control -t upload
// Then:              pio device monitor -e motor_control
//
// Commands (send via Serial, then Enter):
//   g  - start spinning
//   ' ' (space) - stop
//   r  - reverse direction (works whether running or stopped -
//        if running, direction flips immediately; if stopped,
//        the new direction takes effect on the next 'g')
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

void reverseDirection() {
  directionForward = !directionForward;
  if (motorRunning) {
    applyDirection(); // apply immediately since already spinning
  }
  Serial.print(">>> DIRECTION set to ");
  Serial.print(directionForward ? "clockwise" : "anti-clockwise");
  Serial.println(motorRunning ? " (applied now)" : " (applies on next 'g')");
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // wake the driver - REQUIRED or nothing spins
  analogWrite(MOTOR_PWM_PIN, 0);       // start OFF
  applyDirection();

  Serial.println("=== Motor Control ===");
  Serial.println("g = start | space = stop | r = reverse direction");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g' || c == 'G') {
      if (!motorRunning) startMotor();
    } else if (c == ' ') {
      if (motorRunning) stopMotor();
    } else if (c == 'r' || c == 'R') {
      reverseDirection();
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
