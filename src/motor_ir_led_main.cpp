// ============================================================
// MOTOR + IR RECEIVER TEST (with LED indicators)
// Build/upload with: pio run -e motor_ir_led -t upload
// Then:              pio device monitor -e motor_ir_led
//
// Behaviour: motor spins continuously by default. The moment the
// IR receiver detects a signal, the motor stops. Two LEDs give a
// visual readout independent of the motor itself:
//   - WHITE LED: lit whenever the IR receiver currently detects
//     a signal (mirrors exactly what's making the motor stop)
//   - GREEN LED: lit whenever the motor is currently spinning
// These two should always be OPPOSITE of each other. If the white
// LED is lit constantly even with nothing near the receiver, your
// IR pin is floating/miswired.
//
// WIRING:
//   White LED: Uno pin -> resistor -> LED anode, LED cathode -> GND
//   Green LED: Uno pin -> resistor -> LED anode, LED cathode -> GND
//
// IR_SENSOR_PIN uses INPUT_PULLUP so an unconnected/floating pin
// defaults to HIGH (= "not detected"). Most demodulating IR
// receiver modules (TSOP382x etc.) are active-LOW: output goes
// LOW when a signal is detected. If yours is active-HIGH instead,
// flip IR_ACTIVE_STATE below.
// ============================================================
#include <Arduino.h>

const int MOTOR_PWM_PIN   = 9;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_SLEEP_PIN = 7;

const int IR_SENSOR_PIN  = 2;
const int WHITE_LED_PIN  = 4; // lit = IR signal currently detected
const int GREEN_LED_PIN  = 5; // lit = motor currently spinning

const int IR_ACTIVE_STATE = LOW; // change to HIGH if your module is active-HIGH
const int MOTOR_SPEED = 200;

bool motorCurrentlyRunning = false;

void startMotor() {
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  analogWrite(MOTOR_PWM_PIN, MOTOR_SPEED);
  digitalWrite(GREEN_LED_PIN, HIGH);
  motorCurrentlyRunning = true;
  Serial.println(">>> No IR signal - motor SPINNING (green LED on) <<<");
}

void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(GREEN_LED_PIN, LOW);
  motorCurrentlyRunning = false;
  Serial.println(">>> IR signal detected - motor STOPPED (white LED on) <<<");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(WHITE_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // enable driver
  digitalWrite(WHITE_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  Serial.println("=== Motor + IR Receiver Test (with LEDs) ===");
  Serial.println("White LED = IR detected | Green LED = motor spinning");
  Serial.println("These should always be opposite states.");
  Serial.println();

  bool startupState = (digitalRead(IR_SENSOR_PIN) == IR_ACTIVE_STATE);
  Serial.print("Startup IR pin check: ");
  Serial.println(startupState ? "DETECTED (white LED should be on now)" : "clear (not detected)");
  digitalWrite(WHITE_LED_PIN, startupState ? HIGH : LOW);
  delay(1500);

  startMotor();
}

void loop() {
  bool irDetected = (digitalRead(IR_SENSOR_PIN) == IR_ACTIVE_STATE);
  digitalWrite(WHITE_LED_PIN, irDetected ? HIGH : LOW);

  if (irDetected && motorCurrentlyRunning) {
    stopMotor();
  } else if (!irDetected && !motorCurrentlyRunning) {
    startMotor();
  }

  delay(20); // small debounce
}
