#include "LaserControl.h"
#include "Config.h"

LaserControl::LaserControl() : _firing(false), _dwelling(false), _dwellStartMs(0) {}

void LaserControl::begin() {
  pinMode(LASER_PIN, OUTPUT);
  off();
}

void LaserControl::fire() {
  digitalWrite(LASER_PIN, HIGH);
  _firing = true;
}

void LaserControl::off() {
  digitalWrite(LASER_PIN, LOW);
  _firing = false;
  _dwelling = false;
}

bool LaserControl::isFiring() const {
  return _firing;
}

bool LaserControl::updateDwell(bool onTarget) {
  if (!_firing) return false;

  if (onTarget) {
    if (!_dwelling) {
      _dwelling = true;
      _dwellStartMs = millis();
    }
    if (millis() - _dwellStartMs >= LASER_DWELL_MS) {
      return true; // held the full 2 seconds - scoring event
    }
  } else {
    // moved off target - dwell timer resets (per spec: off-target = no hit)
    _dwelling = false;
  }

  return false;
}

unsigned long LaserControl::getDwellElapsedMs() const {
  if (!_dwelling) return 0;
  return millis() - _dwellStartMs;
}
