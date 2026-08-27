// ============================================================
// REACTIVE IR + MOTOR TEST - Nano V3
// Build/upload with: pio run -e reactive_ir -t upload
// Then:              pio device monitor -e reactive_ir
//
// Behaviour: motor spins continuously by default. The moment the
// IR receiver detects a signal, the motor stops. When the signal
// goes away, the motor resumes automatically.
//
// IR_SENSOR_PIN uses INPUT_PULLUP so an unconnected/floating pin
// defaults to HIGH (= "not detected"). Most demodulating IR
// receiver modules (TSOP382x etc.) are active-LOW. If yours is
// active-HIGH instead, flip IR_ACTIVE_STATE below.
// ============================================================
#include <Arduino.h>

const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;

const int IR_SENSOR_PIN = 2;
const int IR_ACTIVE_STATE = LOW; // change to HIGH if your module is active-HIGH
const int MOTOR_SPEED = 200;

bool motorCurrentlyRunning = false;

void startMotor() {
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PWM_PIN, MOTOR_SPEED);
  motorCurrentlyRunning = true;
  Serial.println(">>> No IR signal - motor SPINNING <<<");
}

void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  motorCurrentlyRunning = false;
  Serial.println(">>> IR signal detected - motor STOPPED <<<");
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable driver

  Serial.println("=== Reactive IR + Motor Test (Nano V3) ===");
  Serial.println("Motor spins by default. IR signal detected -> motor stops.");
  Serial.println();

  delay(1000);
  startMotor();
}

void loop() {
  bool irDetected = (digitalRead(IR_SENSOR_PIN) == IR_ACTIVE_STATE);

  if (irDetected && motorCurrentlyRunning) {
    stopMotor();
  } else if (!irDetected && !motorCurrentlyRunning) {
    startMotor();
  }

  delay(20); // small debounce
}
