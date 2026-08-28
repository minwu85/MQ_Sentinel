// ============================================================
// 3-SENSOR DIRECTIONAL IR TEST - Nano V3
// Build/upload with: pio run -e directional_ir -t upload
// Then:              pio device monitor -e directional_ir
//
// Samples three IR receivers (left/right/centre) for 50ms, counts
// how many times each read LOW (active), and turns the motor
// toward whichever side has the strongest signal. Centre or no
// signal both stop the motor.
//
// FIX from original: added MOTOR_SLEEP_PIN handling. The DRV8874
// driver's nSLEEP pin must be driven HIGH or it stays disabled and
// the motor won't spin no matter what PWM/DIR values you send -
// this was missing from the original snippet.
// ============================================================
#include <Arduino.h>

const int IR_LEFT   = 4;
const int IR_RIGHT  = 5;
const int IR_CENTRE = 6;

const int MOTOR_DIR   = 8;   // DRV8874 IN2 / PH
const int MOTOR_PWM   = 9;   // DRV8874 IN1 / EN
const int MOTOR_SLEEP = 7;   // DRV8874 nSLEEP - must be HIGH to enable (added)

void setup() {
  Serial.begin(9600);

  pinMode(IR_LEFT, INPUT_PULLUP);
  pinMode(IR_RIGHT, INPUT_PULLUP);
  pinMode(IR_CENTRE, INPUT_PULLUP);

  pinMode(MOTOR_DIR, OUTPUT);
  pinMode(MOTOR_PWM, OUTPUT);
  pinMode(MOTOR_SLEEP, OUTPUT);

  digitalWrite(MOTOR_SLEEP, HIGH); // enable the driver - required or nothing spins
  analogWrite(MOTOR_PWM, 0);

  Serial.println("3-sensor directional test ready");
}

void loop() {
  int leftCount = 0;
  int rightCount = 0;
  int centreCount = 0;

  // Sample sensors for about 50 ms
  unsigned long start = millis();
  while (millis() - start < 50) {
    if (digitalRead(IR_LEFT) == LOW) {
      leftCount++;
    }
    if (digitalRead(IR_RIGHT) == LOW) {
      rightCount++;
    }
    if (digitalRead(IR_CENTRE) == LOW) {
      centreCount++;
    }
    delayMicroseconds(200);
  }

  // Nothing detected
  if (leftCount == 0 && rightCount == 0 && centreCount == 0) {
    analogWrite(MOTOR_PWM, 0);
    Serial.println("NO IR - STOP");
  }
  // Centre has strongest signal
  else if (centreCount >= leftCount && centreCount >= rightCount) {
    analogWrite(MOTOR_PWM, 0);
    Serial.println("CENTRE - STOP");
  }
  // Left has strongest signal
  else if (leftCount > rightCount) {
    digitalWrite(MOTOR_DIR, LOW);
    analogWrite(MOTOR_PWM, 140);
    Serial.println("LEFT - TURN");
  }
  // Right has strongest signal
  else {
    digitalWrite(MOTOR_DIR, HIGH);
    analogWrite(MOTOR_PWM, 140);
    Serial.println("RIGHT - TURN");
  }

  delay(50);
}
