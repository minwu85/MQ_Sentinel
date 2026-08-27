// ============================================================
// IR-TRIGGERED MOTOR SPIN - Nano V3
// Build/upload with: pio run -e ir_triggered_spin -t upload
// Then:              pio device monitor -e ir_triggered_spin
//
// Pipeline: Uno (running ir_transmitter_test) emits a 38kHz IR
// signal via an IR LED -> this Nano's IR receiver detects it ->
// once detected even once, the motor starts spinning and KEEPS
// spinning continuously from then on, regardless of whether the
// IR signal is still present. This is a one-way trigger/latch,
// not a live "spin only while detected" toggle - once triggered,
// it stays spinning until the board is reset or powered off.
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

bool triggered = false; // once true, stays true - the "latch"

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable driver
  digitalWrite(MOTOR_DIR_PIN, HIGH);   // clockwise once triggered
  analogWrite(MOTOR_PWM_PIN, 0);       // start OFF - waiting for trigger

  Serial.println("=== IR-Triggered Motor Spin (Nano V3) ===");
  Serial.println("Waiting for IR signal... motor will start and keep spinning once detected.");
}

void loop() {
  if (!triggered) {
    bool irDetected = (digitalRead(IR_SENSOR_PIN) == IR_ACTIVE_STATE);
    if (irDetected) {
      triggered = true;
      analogWrite(MOTOR_PWM_PIN, MOTOR_SPEED);
      Serial.println(">>> IR signal received - motor TRIGGERED, spinning continuously <<<");
    }
  }
  // once triggered, nothing more to do - motor just keeps spinning
}
