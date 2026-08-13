#ifndef IR_SENSOR_H
#define IR_SENSOR_H

#include <Arduino.h>

class IRSensor {
  public:
    IRSensor();

    // Call once in setup()
    void begin();

    // Call every loop() - scans all sensors on the ring
    void update();

    // Returns true if any beacon is currently detected
    bool beaconDetected() const;

    // Returns the index (0..NUM_IR_SENSORS-1) of the sensor with strongest/most
    // recent detection, or -1 if none detected
    int getActiveSensorIndex() const;

    // Rough bearing estimate in degrees (0-359) based on which sensor(s) triggered.
    // Useful as the setpoint for the rotation controller.
    float getBearingEstimate() const;

  private:
    bool _sensorState[8];   // supports up to 8 sensors, only NUM_IR_SENSORS used
    int _activeIndex;
};

#endif
