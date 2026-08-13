// ============================================================
// MOTOR + ENCODER CALIBRATION TEST
// Build/upload with: pio run -e motor_test -t upload
// Purpose: find real encoder counts-per-revolution, verify
// direction control, and get a feel for reaction wheel inertia.
// No IR sensor or laser hardware required for this test.
// ============================================================
#include <Arduino.h>
#include "Config.h"
#include "MotorControl.h"

MotorControl motor;

void encoderISR() {
  bool bState = digitalRead(ENCODER_B_PIN);
  motor.onEncoderTick(bState);
}

// Edit these to run different test phases
const int TEST_SPEEDS[] = {100, 150, 200, 255, -100, -200};
const int NUM_TEST_SPEEDS = sizeof(TEST_SPEEDS) / sizeof(TEST_SPEEDS[0]);
const unsigned long RUN_TIME_MS = 3000;   // how long to hold each speed
const unsigned long BRAKE_TIME_MS = 2000; // how long to observe coast/brake after stopping

void setup() {
  Serial.begin(9600);
  Serial.println("=== Motor + Encoder Calibration Test ===");

  motor.begin();
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);

  Serial.println("Manually rotate the shaft exactly ONE full revolution by hand");
  Serial.println("(motor unpowered) and note the encoder count printed below.");
  Serial.println("That number is your real COUNTS_PER_REV - update MotorControl.cpp with it.");
  Serial.println();
  delay(3000);

  motor.resetEncoder();
}

void loop() {
  static int testIndex = 0;

  if (testIndex < NUM_TEST_SPEEDS) {
    int speed = TEST_SPEEDS[testIndex];

    Serial.print("--- Testing speed: ");
    Serial.print(speed);
    Serial.println(" ---");

    motor.resetEncoder();
    motor.setSpeed(speed);

    unsigned long start = millis();
    while (millis() - start < RUN_TIME_MS) {
      // print encoder count every 250ms while running
      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 250) {
        lastPrint = millis();
        Serial.print("  encoder count: ");
        Serial.println(motor.getEncoderCount());
      }
    }

    long countsAtFullSpeed = motor.getEncoderCount();
    motor.stop();

    Serial.print("Speed ");
    Serial.print(speed);
    Serial.print(" -> counts after ");
    Serial.print(RUN_TIME_MS);
    Serial.print("ms run: ");
    Serial.println(countsAtFullSpeed);

    // Observe coast/overshoot after cutting power - this tells you how much
    // braking logic you'll need once the flywheel is mounted.
    long countBeforeBrakeWatch = motor.getEncoderCount();
    delay(BRAKE_TIME_MS);
    long countAfterBrakeWatch = motor.getEncoderCount();
    Serial.print("  Coast after stop: ");
    Serial.print(countAfterBrakeWatch - countBeforeBrakeWatch);
    Serial.println(" counts of drift/overshoot");
    Serial.println();

    delay(1000); // pause between test speeds
    testIndex++;
  } else {
    // all tests done, idle
    static bool printedDone = false;
    if (!printedDone) {
      Serial.println("=== All test speeds complete ===");
      printedDone = true;
    }
  }
}
