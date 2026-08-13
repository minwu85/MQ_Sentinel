#include "MotorControl.h"
#include "Config.h"

MotorControl::MotorControl() : _encoderCount(0), _currentSpeed(0) {}

void MotorControl::begin() {
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  digitalWrite(MOTOR_SLEEP_PIN, HIGH); // wake the DRV8874 driver
  stop();

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);

  // Attach your encoder interrupt in main.cpp with attachInterrupt(), since
  // pin-to-ISR binding needs a free function, not a class member directly.
  // See main.cpp for the wiring example.
}

void MotorControl::setSpeed(int speed) {
  speed = constrain(speed, -255, 255);
  _currentSpeed = speed;

  digitalWrite(MOTOR_DIR_PIN, speed >= 0 ? HIGH : LOW);
  analogWrite(MOTOR_PWM_PIN, abs(speed));
}

void MotorControl::stop() {
  setSpeed(0);
}

void MotorControl::onEncoderTick(bool bHigh) {
  // Quadrature decode: direction determined by state of channel B when A ticks
  if (bHigh) {
    _encoderCount++;
  } else {
    _encoderCount--;
  }
}

long MotorControl::getEncoderCount() const {
  return _encoderCount;
}

void MotorControl::resetEncoder() {
  _encoderCount = 0;
}

float MotorControl::getHeadingEstimate() const {
  // Placeholder conversion - replace COUNTS_PER_REV with your actual encoder
  // resolution and any gear/wheel ratio between reaction wheel and satellite body.
  const float COUNTS_PER_REV = 1000.0f; // TODO: confirm from FIT0186 datasheet
  float revs = _encoderCount / COUNTS_PER_REV;
  float degrees = fmod(revs * 360.0f, 360.0f);
  if (degrees < 0) degrees += 360.0f;
  return degrees;
}
