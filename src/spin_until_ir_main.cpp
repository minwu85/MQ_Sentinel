// ============================================================
// SPIN UNTIL IR (v0.2) - 3x TSOP4138 - Nano V3
// Build/upload with: pio run -e spin_until_ir -t upload
// Then:              pio device monitor -e spin_until_ir
//
// Behaviour: the motor spins continuously. As soon as ANY of the
// three IR receivers (S1=D4, S2=D5, S3=D6) detects the 38 kHz
// signal, the motor stops. When the signal has been gone for
// CLEAR_WINDOWS_TO_RESUME windows, the motor resumes spinning
// (set LATCH_ON_DETECT = true to stay stopped until reset instead).
//
// Detection: each sensor is sampled for SAMPLE_WINDOW_MS and the
// number of LOW reads (TSOP output is active-LOW) is counted. A
// sensor is "detecting" when its count >= DETECT_THRESHOLD.
// Maths and tuning notes: IR_Spin_Stop_Math.md
// ============================================================
#include <Arduino.h>

const int S1_PIN = 4;
const int S2_PIN = 5;
const int S3_PIN = 6;

const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_PWM_PIN   = 9;

const int MOTOR_SPEED = 200;                 // PWM 0-255
const unsigned long SAMPLE_WINDOW_MS = 100;  // one detection window
const int DETECT_THRESHOLD = 5;              // LOW reads needed in a window
const int CLEAR_WINDOWS_TO_RESUME = 2;       // quiet windows before spinning again
const bool LATCH_ON_DETECT = false;          // true = stay stopped after first detection

bool motorRunning = false;
int clearWindows = 0;

void startMotor() {
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PWM_PIN, MOTOR_SPEED);
  motorRunning = true;
  Serial.println(">>> SPINNING <<<");
}

void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  motorRunning = false;
  Serial.println(">>> IR DETECTED - STOPPED <<<");
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable DRV8874
  analogWrite(MOTOR_PWM_PIN, 0);

  pinMode(S1_PIN, INPUT_PULLUP);
  pinMode(S2_PIN, INPUT_PULLUP);
  pinMode(S3_PIN, INPUT_PULLUP);

  Serial.println("=== Spin Until IR v0.2 (S1=D4, S2=D5, S3=D6) ===");
  delay(1000);
  startMotor();
}

void loop() {
  int s1 = 0, s2 = 0, s3 = 0;

  unsigned long startTime = millis();
  while (millis() - startTime < SAMPLE_WINDOW_MS) {
    if (digitalRead(S1_PIN) == LOW) s1++;
    if (digitalRead(S2_PIN) == LOW) s2++;
    if (digitalRead(S3_PIN) == LOW) s3++;
    delayMicroseconds(100);
  }

  bool detected = (s1 >= DETECT_THRESHOLD) ||
                  (s2 >= DETECT_THRESHOLD) ||
                  (s3 >= DETECT_THRESHOLD);

  Serial.print("S1(D4): "); Serial.print(s1);
  Serial.print("  S2(D5): "); Serial.print(s2);
  Serial.print("  S3(D6): "); Serial.print(s3);
  Serial.println(detected ? "  DETECTED" : "  --");

  if (detected) {
    clearWindows = 0;
    if (motorRunning) {
      stopMotor();
    }
  } else if (!motorRunning && !LATCH_ON_DETECT) {
    clearWindows++;
    if (clearWindows >= CLEAR_WINDOWS_TO_RESUME) {
      startMotor();
    }
  }
}
