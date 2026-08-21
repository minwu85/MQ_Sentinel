// ============================================================
// MOTOR FORWARD/BACKWARD SEQUENCE TEST
// Build/upload with: pio run -e motor_sequence -t upload
//
// Behaviour: sends "g" over Serial to trigger the sequence:
//   - Spin FORWARD for 5 seconds
//   - Stop briefly
//   - Spin BACKWARD for 5 seconds
//   - Stop
// Then waits idle until you send "g" again. Doesn't loop on its
// own - runs once per command, as requested.
// ============================================================
#include <Arduino.h>

<<<<<<< HEAD
const int MOTOR_PWM_PIN   = 9;//
const int MOTOR_DIR_PIN   = 8;//
const int MOTOR_SLEEP_PIN = 7;//
=======
const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;
const int LED_PIN = 13; // onboard LED - gives a visual cue independent of the motor itself
>>>>>>> d314bd078ca08b0190add54a1f324e04ce7d68a0

const int SPIN_SPEED = 200;              // 0-255
const unsigned long SPIN_DURATION_MS = 10000UL; // 10 seconds each direction
const unsigned long PAUSE_BETWEEN_MS = 1500UL;  // longer pause so the direction change is obvious

void blinkLED(int times, int gapMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(gapMs);
    digitalWrite(LED_PIN, LOW);
    delay(gapMs);
  }
}

void runSequence() {
<<<<<<< HEAD
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);
=======
  Serial.println();
  Serial.println("========================================");
>>>>>>> d314bd078ca08b0190add54a1f324e04ce7d68a0
  Serial.println("Sequence started.");
  Serial.println("========================================");

  Serial.println();
  Serial.println(">>> CLOCKWISE (forward) - 10 seconds <<<");
  blinkLED(1, 150); // one blink = clockwise
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED);
  delay(SPIN_DURATION_MS);

  Serial.println("Stop - settling before direction change");
  analogWrite(MOTOR_PWM_PIN, 0);
  delay(PAUSE_BETWEEN_MS);

  Serial.println();
  Serial.println(">>> ANTI-CLOCKWISE (reverse) - 10 seconds <<<");
  blinkLED(2, 150); // two blinks = anti-clockwise
  digitalWrite(MOTOR_DIR_PIN, LOW);
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED);
  delay(SPIN_DURATION_MS);

  Serial.println("Stop");
  analogWrite(MOTOR_PWM_PIN, 0);

  Serial.println();
  Serial.println("Sequence complete. Send 'g' to run again.");
  Serial.println("========================================");
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable driver
  analogWrite(MOTOR_PWM_PIN, 0);

  Serial.println("=== Motor Forward/Backward Sequence Test ===");
  Serial.println("Send 'g' (then Enter) to run: clockwise 10s, anti-clockwise 10s");
  Serial.println("LED cue: 1 blink = clockwise starting, 2 blinks = anti-clockwise starting");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g' || c == 'G') {
      runSequence();
    }
  }
}
