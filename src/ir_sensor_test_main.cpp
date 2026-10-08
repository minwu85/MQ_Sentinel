// ============================================================
// 3 IR SENSOR TEST (v0.1) - Nano V3
// Build/upload with: pio run -e ir_sensor_test -t upload
// Then:              pio device monitor -e ir_sensor_test
//
// S1 = D4, S2 = D5, S3 = D6. Motor driver is held asleep so the
// motor cannot move during this test. Prints the LOW-read count for
// each sensor over a 100 ms window; count > 5 is flagged DETECTED.
// Use these counts to choose DETECT_THRESHOLD for spin_until_ir.
// ============================================================
#include <Arduino.h>

const int S1_PIN = 4;
const int S2_PIN = 5;
const int S3_PIN = 6;

const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_PWM_PIN   = 9;

void setup() {
  Serial.begin(9600);

  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);

  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(MOTOR_DIR_PIN, LOW);
  digitalWrite(MOTOR_SLEEP_PIN, LOW); // driver asleep

  pinMode(S1_PIN, INPUT_PULLUP);
  pinMode(S2_PIN, INPUT_PULLUP);
  pinMode(S3_PIN, INPUT_PULLUP);

  Serial.println("3 IR SENSOR TEST");
  Serial.println("S1 = D4");
  Serial.println("S2 = D5");
  Serial.println("S3 = D6");
  Serial.println();
}

void loop() {
  int s1Count = 0;
  int s2Count = 0;
  int s3Count = 0;

  unsigned long startTime = millis();
  while (millis() - startTime < 100) {
    if (digitalRead(S1_PIN) == LOW) s1Count++;
    if (digitalRead(S2_PIN) == LOW) s2Count++;
    if (digitalRead(S3_PIN) == LOW) s3Count++;
    delayMicroseconds(100);
  }

  Serial.print("D4 / S1: ");
  Serial.print(s1Count);
  Serial.print(s1Count > 5 ? " DETECTED" : " --");
  Serial.print("    ");

  Serial.print("D5 / S2: ");
  Serial.print(s2Count);
  Serial.print(s2Count > 5 ? " DETECTED" : " --");
  Serial.print("    ");

  Serial.print("D6 / S3: ");
  Serial.print(s3Count);
  Serial.print(s3Count > 5 ? " DETECTED" : " --");
  Serial.println();

  delay(200);
}
