// ============================================================
// NANO V3 <-> MOTOR CONNECTIVITY TEST
// Build/upload with: pio run -e connection_test -t upload
// Then:              pio device monitor -e connection_test
//
// Purpose: prove the full signal chain actually works, not just
// "the motor looked like it spun". Pulses the motor briefly and
// measures encoder count before/after in both directions.
// Reply: 1 = motor confirmed connected, 0 = not connected
// ============================================================
#include <Arduino.h>

const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;

const int ENCODER_A_PIN = 2;
const int ENCODER_B_PIN = 3;

volatile long encoderCount = 0;

void encoderISR() {
  bool bState = digitalRead(ENCODER_B_PIN);
  if (bState) {
    encoderCount++;
  } else {
    encoderCount--;
  }
}

void pulseMotor(bool forward, int speed, unsigned long durationMs) {
  digitalWrite(MOTOR_DIR_PIN, forward ? HIGH : LOW);
  analogWrite(MOTOR_PWM_PIN, speed);
  delay(durationMs);
  analogWrite(MOTOR_PWM_PIN, 0);
}

void runDirectionTest(const char* label, bool forward) {
  Serial.print(label);
  Serial.print(": ");

  long before = encoderCount;
  pulseMotor(forward, 200, 500);
  delay(100);
  long after = encoderCount;
  long delta = after - before;

  if (delta != 0) {
    Serial.println("1  (connected - encoder moved)");
  } else {
    Serial.println("0  (NOT connected - no encoder movement)");
  }
}

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("=== Nano V3 <-> Motor Connectivity Test ===");
  Serial.println("Reply: 1 = motor confirmed connected, 0 = not connected");
  Serial.println();

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable driver
  analogWrite(MOTOR_PWM_PIN, 0);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);

  Serial.print("Encoder A idle state: ");
  Serial.println(digitalRead(ENCODER_A_PIN) ? "HIGH" : "LOW");
  Serial.print("Encoder B idle state: ");
  Serial.println(digitalRead(ENCODER_B_PIN) ? "HIGH" : "LOW");
  Serial.println();

  delay(1000);
}

void loop() {
  runDirectionTest("FORWARD", true);
  delay(1000);

  runDirectionTest("REVERSE", false);
  delay(1000);

  Serial.println("=== Test cycle complete. Repeating in 5 seconds... ===");
  Serial.println();
  delay(5000);
}
