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

const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;

const int SPIN_SPEED = 200;              // 0-255
const unsigned long SPIN_DURATION_MS = 5000UL; // 5 seconds each direction
const unsigned long PAUSE_BETWEEN_MS = 500UL;  // brief stop between directions

void runSequence() {
  Serial.println("Sequence started.");

  Serial.println("Forward - 5 seconds");
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED);
  delay(SPIN_DURATION_MS);

  Serial.println("Stop");
  analogWrite(MOTOR_PWM_PIN, 0);
  delay(PAUSE_BETWEEN_MS);

  Serial.println("Backward - 5 seconds");
  digitalWrite(MOTOR_DIR_PIN, LOW);
  analogWrite(MOTOR_PWM_PIN, SPIN_SPEED);
  delay(SPIN_DURATION_MS);

  Serial.println("Stop");
  analogWrite(MOTOR_PWM_PIN, 0);

  Serial.println("Sequence complete. Send 'g' to run again.");
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable driver
  analogWrite(MOTOR_PWM_PIN, 0);

  Serial.println("=== Motor Forward/Backward Sequence Test ===");
  Serial.println("Send 'g' (then Enter) to run: forward 5s, backward 5s");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g' || c == 'G') {
      runSequence();
    }
  }
}
