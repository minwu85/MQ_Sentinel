#include "IRSensor.h"
#include "Config.h"

IRSensor::IRSensor() : _activeIndex(-1) {
  for (int i = 0; i < 8; i++) _sensorState[i] = false;
}

void IRSensor::begin() {
  for (int i = 0; i < NUM_IR_SENSORS; i++) {
    // Demodulating IR receiver modules (e.g. TSOP38xx) are active-LOW
    // and already strip the 38 kHz carrier internally.
    pinMode(IR_SENSOR_PINS[i], INPUT);
  }
}

void IRSensor::update() {
  _activeIndex = -1;

  for (int i = 0; i < NUM_IR_SENSORS; i++) {
    // Active-low: LOW means beacon detected
    _sensorState[i] = (digitalRead(IR_SENSOR_PINS[i]) == LOW);

    if (_sensorState[i] && _activeIndex == -1) {
      _activeIndex = i;
    }
  }
}

bool IRSensor::beaconDetected() const {
  return _activeIndex != -1;
}

int IRSensor::getActiveSensorIndex() const {
  return _activeIndex;
}

float IRSensor::getBearingEstimate() const {
  if (_activeIndex == -1) return -1.0f;

  // Sensors assumed evenly spaced around the ring (360 / NUM_IR_SENSORS apart).
  float spacing = 360.0f / NUM_IR_SENSORS;
  return _activeIndex * spacing;
}
