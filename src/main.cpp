#include <Arduino.h>
#include "Config.h"
#include "IRSensor.h"
#include "MotorControl.h"
#include "LaserControl.h"

IRSensor irSensor;
MotorControl motor;
LaserControl laser;

enum State {
  STATE_IDLE,        // no beacon detected, wheel stationary
  STATE_SEEKING,      // beacon detected, rotating to align
  STATE_ALIGNED,       // laser on target, timing the 2s dwell
  STATE_SCORED        // dwell complete, brief pause before re-seeking
};

State currentState = STATE_IDLE;
unsigned long demoStartMs = 0;

// --- Encoder ISR wiring ---
// attachInterrupt needs a free function, so we bridge to the MotorControl instance here.
void encoderISR() {
  bool bState = digitalRead(ENCODER_B_PIN);
  motor.onEncoderTick(bState);
}

void setup() {
  Serial.begin(9600);
  Serial.println("MQ Sentinel booting...");

  irSensor.begin();
  motor.begin();
  laser.begin();

  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);

  demoStartMs = millis();
  Serial.println("MQ Sentinel ready.");
}

void loop() {
  irSensor.update();

  // Placeholder alignment check - replace with your real logic once you
  // define how "aligned" is measured (heading estimate vs bearing estimate,
  // within some tolerance).
  float bearing = irSensor.getBearingEstimate();
  float heading = motor.getHeadingEstimate();
  float error = bearing - heading;
  bool aligned = irSensor.beaconDetected() && fabs(error) < 3.0f; // 3 degree tolerance - tune this

  switch (currentState) {

    case STATE_IDLE:
      motor.stop();
      if (irSensor.beaconDetected()) {
        currentState = STATE_SEEKING;
      }
      break;

    case STATE_SEEKING: {
      if (!irSensor.beaconDetected()) {
        currentState = STATE_IDLE;
        break;
      }
      // Simple proportional control - tune kP for your reaction wheel inertia
      const float kP = 4.0f;
      int speed = constrain((int)(error * kP), -255, 255);
      motor.setSpeed(speed);

      if (aligned) {
        motor.stop();
        laser.fire();
        currentState = STATE_ALIGNED;
      }
      break;
    }

    case STATE_ALIGNED:
      if (laser.updateDwell(aligned)) {
        Serial.println("HIT - 2s dwell complete");
        currentState = STATE_SCORED;
      } else if (!aligned) {
        // fell off target before completing dwell
        laser.off();
        currentState = STATE_SEEKING;
      }
      break;

    case STATE_SCORED:
      laser.off();
      currentState = STATE_IDLE;
      break;
  }

  // End demonstration after 5 minutes
  if (millis() - demoStartMs >= DEMO_DURATION_MS) {
    motor.stop();
    laser.off();
    Serial.println("Demonstration time expired.");
    while (true) { /* halt */ }
  }
}
