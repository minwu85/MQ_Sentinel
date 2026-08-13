#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

class MotorControl {
  public:
    MotorControl();

    void begin();

    // Drive the reaction wheel motor. speed: -255 (full reverse) to 255 (full forward)
    void setSpeed(int speed);

    void stop();

    // Call from an interrupt (attachInterrupt) on the encoder A channel
    void onEncoderTick(bool bHigh);

    // Current estimated heading in degrees (0-359), derived from encoder counts.
    // NOTE: this tracks REACTION WHEEL rotation, not satellite body rotation directly -
    // you'll need a conversion factor based on your reaction wheel / body inertia ratio,
    // or a separate body-mounted heading reference (e.g. IMU) for real closed-loop control.
    float getHeadingEstimate() const;

    long getEncoderCount() const;
    void resetEncoder();

  private:
    volatile long _encoderCount;
    int _currentSpeed;
};

#endif
