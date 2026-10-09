// ============================================================
// SPIN DETECT (v1.1) - simple rewrite of spin_until_ir, SLOW speed, laser ON = motor OFF - Nano V3
// Build/upload with: pio run -e spin_detect -t upload
// Then:              pio device monitor -e spin_detect     (115200 baud)
//
// Same idea as spin_until_ir: spin, count LOW reads from the sensors in
// a window, stop when the signal is there. Added:
//   * three sensors, each with its own turn direction
//   * after a detection the motor turns clockwise / anti-clockwise
//     (depending on WHICH sensor saw it) until S1 - the sensor on the
//     laser axis - sees it too, then it stops
//   * every stop is STOP_MS (4 s) long and the laser is ON for all of it
//   * LASER RULE: laser ON whenever the motor is OFF (power-up and every stop),
//     laser OFF whenever the motor is spinning or turning
//   * slow speed: SPIN_SPEED 130, TRACK_SPEED 110 (was 200 / 150)
//
//   SCAN (CW, laser OFF) -> detected -> [S1 strongest: STOP]
//                           else TURN toward the strongest sensor
//   TURN -> S1 aligned -> STOP (laser ON, 4 s) -> SCAN again
//
// Output (every window):
//   S1(D4): 72  S2(D5): 71  S3(D6): 71  DETECTED | Acc(g) X:-0.68 Y:-0.60 Z:0.51 | Gyro(dps) X:-5.7 Y:2.7 Z:-3.8 | SCAN CW
//
// MPU6050 (display only): VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68).
// Maths, wiring, tuning: spin_detect.md
// ============================================================
#include <Arduino.h>
#include <Wire.h>

// ---------- Pins ----------
const int NUM_SENSORS = 3;
const int SENSOR_PINS[NUM_SENSORS] = {4, 5, 6};   // S1, S2, S3 (TSOP4138, active-LOW)

const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;   // HIGH = clockwise (as motor_onoff)
const int MOTOR_PWM_PIN   = 9;
const int LASER_PIN       = 10;  // HIGH = laser ON

// ---------- Sensor -> turn direction (CONFIGURE to your wiring) ----------
// +1 = turn clockwise, -1 = turn anti-clockwise, 0 = this sensor is on the laser axis (aligned).
// Default: S1 on the laser axis, S2 120 deg clockwise of it, S3 120 deg anti-clockwise of it.
const int SENSOR_TURN[NUM_SENSORS] = {0, +1, -1};
const int CENTRE_SENSOR = 0;                      // index of the sensor on the laser axis

// ---------- Motor ----------
const int SPIN_SPEED = 130;                       // scanning PWM - SLOW (was 200); raise if the motor stalls
const int TRACK_SPEED = 110;                      // PWM while turning toward the target - SLOW (was 150)
const bool SCAN_CLOCKWISE = true;
const unsigned long REVERSE_PAUSE_MS = 100;       // pause before the motor changes direction

// ---------- Detection (spin_until_ir approach) ----------
const unsigned long WINDOW_MS = 100;              // one counting window
const int DETECT_THRESHOLD = 5;                   // LOW reads in a window = signal (spin_until_ir value)
const int CONFIRM_WINDOWS = 1;                    // consecutive windows needed (2 = more noise immunity)
const int CLEAR_WINDOWS_TO_REARM = 2;             // quiet windows before the next detection
const int ALIGN_MARGIN = 8;                       // S1 counts as aligned if within this of the strongest

// ---------- Turn / stop ----------
const unsigned long TRACK_TIMEOUT_MS = 4000;      // give up turning -> stop here
const int LOST_WINDOWS = 5;                       // no signal this many windows while turning -> back to scanning
const unsigned long STOP_MS = 4000;               // stop time, laser ON for all of it

// ---------- MPU6050 (display only) ----------
const uint8_t MPU_ADDR = 0x68;
const float GYRO_LSB = 32.8f;                     // +/-1000 deg/s range
const unsigned long IMU_RETRY_MS = 2000;

// ---------- State ----------
enum State { ST_SCAN, ST_TURN, ST_STOP };
State state = ST_SCAN;

int counts[NUM_SENSORS];
bool armed = false;
int clearRun = 0;
int detectRun = 0;
int alignedRun = 0;
int lostRun = 0;

int motorDir = 1;                                 // +1 clockwise, -1 anti-clockwise
int motorPwm = 0;
unsigned long turnStartMs = 0;
unsigned long stopEndMs = 0;

bool imuOk = false;
unsigned long lastImuTryMs = 0;

// ---------- Laser / motor ----------
void laserSet(bool on) {
  digitalWrite(LASER_PIN, on ? HIGH : LOW);
}

void driveMotor(int dir, int speed) {
  if (motorPwm > 0 && dir != motorDir) {          // reverse safely: stop, pause, go
    analogWrite(MOTOR_PWM_PIN, 0);
    delay(REVERSE_PAUSE_MS);
  }
  digitalWrite(MOTOR_DIR_PIN, dir > 0 ? HIGH : LOW);
  laserSet(false);                                // motor runs -> laser OFF (before the motor starts)
  analogWrite(MOTOR_PWM_PIN, speed);
  motorDir = dir;
  motorPwm = speed;
}

void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  motorPwm = 0;
  laserSet(true);                                 // motor OFF -> laser ON (after the motor has stopped)
}

// ---------- MPU6050 ----------
bool imuInit() {
  Wire.begin();
  Wire.setClock(400000);
#ifdef WIRE_HAS_TIMEOUT
  Wire.setWireTimeout(3000, true);
#endif
  Wire.beginTransmission(MPU_ADDR);
  Wire.write((uint8_t)0x6B);                      // PWR_MGMT_1: wake up
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) return false;  // no ACK at 0x68
  delay(50);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write((uint8_t)0x1B);                      // GYRO_CONFIG: +/-1000 deg/s
  Wire.write((uint8_t)0x10);
  return Wire.endTransmission() == 0;
}

bool imuRead(float a[3], float g[3]) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write((uint8_t)0x3B);                      // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)14) != 14) return false;
  int16_t v[7];
  for (int i = 0; i < 7; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    v[i] = (int16_t)(((uint16_t)hi << 8) | lo);
  }
  for (int i = 0; i < 3; i++) {
    a[i] = (float)v[i] / 16384.0f;                // +/-2 g
    g[i] = (float)v[4 + i] / GYRO_LSB;
  }
  return true;
}

// ---------- Counting window (same as spin_until_ir) ----------
void sampleWindow(unsigned long ms) {
  for (int i = 0; i < NUM_SENSORS; i++) counts[i] = 0;
  unsigned long start = millis();
  while (millis() - start < ms) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (digitalRead(SENSOR_PINS[i]) == LOW) counts[i]++;
    }
    delayMicroseconds(100);
  }
}

// ---------- Output ----------
void printLine(bool detected, unsigned long now) {
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(F("S"));
    Serial.print(i + 1);
    Serial.print(F("(D"));
    Serial.print(SENSOR_PINS[i]);
    Serial.print(F("): "));
    Serial.print(counts[i]);
    Serial.print(F("  "));
  }
  Serial.print(detected ? F("DETECTED") : F("--"));

  if (!imuOk && now - lastImuTryMs >= IMU_RETRY_MS) {
    lastImuTryMs = now;
    imuOk = imuInit();
  }
  float a[3], g[3];
  if (imuOk && imuRead(a, g)) {
    Serial.print(F(" | Acc(g) X:"));
    Serial.print(a[0], 2);
    Serial.print(F(" Y:"));
    Serial.print(a[1], 2);
    Serial.print(F(" Z:"));
    Serial.print(a[2], 2);
    Serial.print(F(" | Gyro(dps) X:"));
    Serial.print(g[0], 1);
    Serial.print(F(" Y:"));
    Serial.print(g[1], 1);
    Serial.print(F(" Z:"));
    Serial.print(g[2], 1);
  } else {
    imuOk = false;
    Serial.print(F(" | IMU:OFF"));
  }

  Serial.print(F(" | "));
  switch (state) {
    case ST_SCAN:
      Serial.print(F("SCAN "));
      Serial.print(motorDir > 0 ? F("CW") : F("CCW"));
      if (!armed) Serial.print(F(" arming"));
      break;
    case ST_TURN:
      Serial.print(F("TURN "));
      Serial.print(motorDir > 0 ? F("CW") : F("CCW"));
      break;
    case ST_STOP: {
      long left = (long)(stopEndMs - now);
      Serial.print(F("STOP "));
      Serial.print((left > 0 ? left : 0) / 1000.0f, 1);
      Serial.print(F("s LASER ON"));
      break;
    }
  }
  Serial.println();
}

// ---------- State changes ----------
void startScan() {
  laserSet(false);                                // laser OFF before the motor moves
  driveMotor(SCAN_CLOCKWISE ? 1 : -1, SPIN_SPEED);
  state = ST_SCAN;
  detectRun = 0;
}

void startStop(const __FlashStringHelper *why) {
  stopMotor();                                    // motor stops first ...
  laserSet(true);                                 // ... then the laser shoots for the whole stop
  stopEndMs = millis() + STOP_MS;
  state = ST_STOP;
  Serial.print(F(">>> STOP ("));
  Serial.print(why);
  Serial.println(F(") - laser ON <<<"));
}

void endStop() {
  laserSet(false);                                // laser OFF, then spin again
  armed = false;                                  // must see the signal clear before the next detection
  clearRun = 0;
  Serial.println(F(">>> STOP finished - laser OFF, scanning <<<"));
  startScan();
}

void startTurn(int sensor) {
  int dir = SENSOR_TURN[sensor];
  turnStartMs = millis();
  lostRun = 0;
  alignedRun = 0;
  state = ST_TURN;
  Serial.print(F(">>> S"));
  Serial.print(sensor + 1);
  Serial.print(F(" detected: turning "));
  Serial.print(dir > 0 ? F("clockwise") : F("anti-clockwise"));
  Serial.println(F(" toward it <<<"));
  driveMotor(dir, TRACK_SPEED);
}

void setup() {
  pinMode(LASER_PIN, OUTPUT);
  laserSet(true);                                 // the motor is off at power-up -> laser ON (rule: laser ON = motor OFF)

  Serial.begin(115200);
  delay(500);

  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);            // wake the driver - required or nothing spins
  analogWrite(MOTOR_PWM_PIN, 0);

  for (int i = 0; i < NUM_SENSORS; i++) pinMode(SENSOR_PINS[i], INPUT_PULLUP);

  Serial.println(F("=== Spin Detect v1.1 (S1=D4 S2=D5 S3=D6, laser=D10, MPU6050=A4/A5) ==="));
  imuOk = imuInit();
  Serial.println(imuOk ? F("MPU6050 found at 0x68") : F("MPU6050 not found at 0x68 (IMU:OFF) - continuing"));
  Serial.println(F("Auto-starting: scan, detect, turn toward the sensor, stop 4 s with laser ON"));
  delay(1000);

  startScan();
}

void loop() {
  unsigned long dur = WINDOW_MS;
  if (state == ST_STOP) {
    long left = (long)(stopEndMs - millis());
    if (left <= 0) {
      endStop();
      return;
    }
    if ((unsigned long)left < dur) dur = left;    // end the stop exactly on time
  }

  sampleWindow(dur);

  int best = 0;
  for (int i = 1; i < NUM_SENSORS; i++) {
    if (counts[i] > counts[best]) best = i;
  }
  bool detected = counts[best] >= DETECT_THRESHOLD;
  bool aligned = detected && counts[CENTRE_SENSOR] >= DETECT_THRESHOLD &&
                 counts[CENTRE_SENSOR] + ALIGN_MARGIN >= counts[best];
  unsigned long now = millis();

  switch (state) {
    case ST_SCAN:
      if (!armed) {
        if (detected) {
          clearRun = 0;
        } else if (++clearRun >= CLEAR_WINDOWS_TO_REARM) {
          armed = true;
        }
      } else if (detected) {
        if (++detectRun >= CONFIRM_WINDOWS) {
          if (aligned) startStop(F("target on S1"));
          else startTurn(best);
        }
      } else {
        detectRun = 0;
      }
      break;

    case ST_TURN:
      if (now - turnStartMs >= TRACK_TIMEOUT_MS) {
        startStop(F("turn timeout"));
      } else if (!detected) {
        if (++lostRun >= LOST_WINDOWS) {
          Serial.println(F(">>> target lost - scanning <<<"));
          startScan();
        }
      } else {
        lostRun = 0;
        if (aligned) {
          if (++alignedRun >= CONFIRM_WINDOWS) startStop(F("aligned with S1"));
        } else {
          alignedRun = 0;
          int dir = SENSOR_TURN[best];            // follow the strongest sensor
          if (dir != 0 && dir != motorDir) {
            Serial.print(F(">>> S"));
            Serial.print(best + 1);
            Serial.println(F(" strongest: reversing <<<"));
            driveMotor(dir, TRACK_SPEED);
          }
        }
      }
      break;

    case ST_STOP:
      break;
  }

  printLine(detected, now);
}
