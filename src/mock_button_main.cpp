// ============================================================
// MOCK BEACON STATE MACHINE TEST
// Build/upload with: pio run -e mock_button -t upload
// Purpose: exercise the FULL seek -> align -> fire -> dwell -> score
// loop using a pushbutton as a stand-in for real IR beacon detection.
// Swap the button check for irSensor.beaconDetected() later - no other
// state machine logic needs to change.
// ============================================================
#include <Arduino.h>
#include "Config.h"
#include "MotorControl.h"
#include "LaserControl.h"

MotorControl motor;
LaserControl laser;

enum State {
  STATE_IDLE,
  STATE_SEEKING,
  STATE_ALIGNED,
  STATE_SCORED
};

State currentState = STATE_IDLE;

void encoderISR() {
  bool bState = digitalRead(ENCODER_B_PIN);
  motor.onEncoderTick(bState);
}

// --- Mock beacon: pressed button = "beacon detected + aligned" ---
// This deliberately skips bearing/heading math so you can test the
// laser-fire + 2s-dwell + state transition logic in isolation first.
// Once that's solid, wire in real seeking behaviour.
bool mockBeaconDetected() {
  return digitalRead(MOCK_BEACON_BUTTON_PIN) == LOW; // active-low with pullup
}

void setup() {
  Serial.begin(9600);
  Serial.println("=== Mock Beacon State Machine Test ===");
  Serial.println("Hold the button to simulate 'beacon detected + aligned'.");
  Serial.println("Release before 2s to test the 'fell off target' path.");
  Serial.println();

  pinMode(MOCK_BEACON_BUTTON_PIN, INPUT_PULLUP);
  motor.begin();
  laser.begin();
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);
}

void loop() {
  bool aligned = mockBeaconDetected(); // stands in for real alignment check

  switch (currentState) {

    case STATE_IDLE:
      motor.stop();
      if (aligned) {
        Serial.println("[IDLE -> SEEKING] beacon detected");
        currentState = STATE_SEEKING;
      }
      break;

    case STATE_SEEKING:
      // In the real system this drives the reaction wheel toward the
      // target bearing. Here we skip straight to "aligned" once the
      // button is held, so you can test firing/dwell logic quickly.
      if (aligned) {
        Serial.println("[SEEKING -> ALIGNED] locked on, firing laser");
        motor.stop();
        laser.fire();
        currentState = STATE_ALIGNED;
      } else {
        Serial.println("[SEEKING -> IDLE] lost beacon before aligning");
        currentState = STATE_IDLE;
      }
      break;

    case STATE_ALIGNED:
      if (laser.updateDwell(aligned)) {
        Serial.println("[ALIGNED -> SCORED] HIT - held 2 seconds on target!");
        currentState = STATE_SCORED;
      } else if (!aligned) {
        Serial.println("[ALIGNED -> SEEKING] fell off target before 2s completed");
        laser.off();
        currentState = STATE_SEEKING;
      } else {
        // still dwelling - show progress
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 300) {
          lastPrint = millis();
          Serial.print("  dwelling... ");
          Serial.print(laser.getDwellElapsedMs());
          Serial.println("ms");
        }
      }
      break;

    case STATE_SCORED:
      laser.off();
      delay(500); // brief pause so you can see the SCORED state in serial
      currentState = STATE_IDLE;
      break;
  }
}
