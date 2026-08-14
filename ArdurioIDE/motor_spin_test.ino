// ============================================================
// MOTOR SPIN TEST - Arduino IDE version
// Board: Arduino Nano
// Purpose: confirm the DRV8874 driver + FIT0186 motor spin,
// and that the encoder is counting.
// ============================================================

// --- Pin definitions (match your wiring) ---
const int MOTOR_PWM_PIN   = 9;   // driver PWM (speed)
const int MOTOR_DIR_PIN   = 8;   // driver DIR (direction)
const int MOTOR_SLEEP_PIN = 7;   // driver nSLEEP - must be HIGH to enable

const int ENCODER_A_PIN = 2;     // encoder channel A (interrupt pin)
const int ENCODER_B_PIN = 3;     // encoder channel B

volatile long encoderCount = 0;

void encoderISR() {
  bool bState = digitalRead(ENCODER_B_PIN);
  if (bState) {
    encoderCount++;
  } else {
    encoderCount--;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("=== Motor Spin Test ===");

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // wake up the driver
  digitalWrite(MOTOR_PWM_PIN, LOW);

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);

  Serial.println("Setup complete. Starting spin test in 2 seconds...");
  delay(2000);
}

void loop() {
  // Spin forward at moderate speed
  Serial.println("Forward, speed 150");
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PWM_PIN, 150);
  printEncoderFor(3000);

  // Stop
  Serial.println("Stop");
  analogWrite(MOTOR_PWM_PIN, 0);
  printEncoderFor(1000);

  // Spin reverse at moderate speed
  Serial.println("Reverse, speed 150");
  digitalWrite(MOTOR_DIR_PIN, LOW);
  analogWrite(MOTOR_PWM_PIN, 150);
  printEncoderFor(3000);

  // Stop before repeating
  Serial.println("Stop");
  analogWrite(MOTOR_PWM_PIN, 0);
  printEncoderFor(2000);
}

// Prints encoder count every 250ms for the given duration (ms)
void printEncoderFor(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    Serial.print("  encoder count: ");
    Serial.println(encoderCount);
    delay(250);
  }
}
