#ifndef LASER_CONTROL_H
#define LASER_CONTROL_H

#include <Arduino.h>

class LaserControl {
  public:
    LaserControl();

    void begin();

    void fire();
    void off();
    bool isFiring() const;

    // Call every loop(). Pass whether the laser is currently considered
    // "on target" (from your alignment/vision logic). Returns true once
    // the laser has been held on target continuously for LASER_DWELL_MS.
    bool updateDwell(bool onTarget);

    unsigned long getDwellElapsedMs() const;

  private:
    bool _firing;
    bool _dwelling;
    unsigned long _dwellStartMs;
};

#endif
