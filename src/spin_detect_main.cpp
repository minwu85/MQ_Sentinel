// ============================================================
// SPIN DETECT (v0.9) - Nano V3
// 3x TSOP4138 @ 120 deg + encoder + MPU6050 gyro + red laser
// Build/upload with: pio run -e spin_detect -t upload
// Then:              pio device monitor -e spin_detect
//
// Base: motor_onoff (auto-start, g / space / r, DIR+PWM+nSLEEP pins)
//       spin_until_ir (window count >= K, clear-before-resume).
//
// Sequence:
//   continuous spin (laser OFF) -> strong message -> confirm (debounce)
//   -> motor stops -> record encoder, gyro, sensor -> bearing
//   -> rotate the SHORTEST way to the target (angle_error in +/-180,
//      laser still OFF) -> stopped at the target -> laser ON
//   -> hold HOLD_MS (5 s) -> laser OFF -> resume spin -> repeat.
// The laser is ON only in the HOLD state, never while searching.
// A compact live telemetry line (IMU + system state) is printed every
// TELEMETRY_MS without blocking the detection loop.
//
// Layout: S1 = D4 @ 0 deg (reference, laser axis), S2 = D5 @ 120,
// S3 = D6 @ 240. Angles increase in the DIR=HIGH (clockwise) direction.
// MPU6050: VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68).
// Gyro logic is enabled only after the MPU6050 ACKs at 0x68, WHO_AM_I
// reads 0x68 and the gyro bias calibration passes.
//
// All geometry/calibration values below are CONFIGURABLE PLACEHOLDERS.
// Several sensors seeing the beacon at once is NOT a fault: the sensor with the most
// LOW reads over the confirming windows wins. Faults restart automatically (3 tries).
// Set ALIGN_ENABLED = false to stop in place instead of rotating onto the target.
// Maths, wiring and bench calibration: spin_detect.md
//
// Serial: g = start/resume (also clears a fault) | space = stop
//         r = reverse scan direction | z = zero | p = status
//         l = laser master on/off
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ---------- Pins ----------
const int NUM_SENSORS = 3;
const int SENSOR_PINS[NUM_SENSORS] = {4, 5, 6};
const char *SENSOR_NAME[NUM_SENSORS] = {"S1(ref)", "S2(right)", "S3(left)"};

const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_PWM_PIN   = 9;
const int LASER_PIN       = 10;
const int ENCODER_A_PIN   = 2;
const int ENCODER_B_PIN   = 3;
// MPU6050 uses the hardware I2C pins: SDA = A4, SCL = A5 (Wire library)

// ---------- Geometry (intended layout - VERIFY on the bench) ----------
const float SENSOR_MOUNT_DEG[NUM_SENSORS] = {0.0f, 120.0f, 240.0f}; // 360 / 3
const float LASER_MOUNT_DEG = 0.0f;       // laser axis angle on the assembly (= S1 axis)
const float EDGE_HALF_DEG   = 45.0f;      // PLACEHOLDER: off-axis angle where a sensor first reaches K
const float SENSOR_RADIUS_MM   = 0.0f;    // sensor distance from centre; 0 = ignore parallax
const float TARGET_DISTANCE_MM = 0.0f;    // sensor-to-target distance; 0 = ignore parallax

// Encoder counts for ONE full turn of the assembly (after gearing).
// 0 = not calibrated: the gyro is then the angle source (if verified).
const long COUNTS_PER_REV = 0;

// ---------- Motor ----------
const int SPIN_SPEED  = 200;              // PWM while scanning (spin_until_ir value)
const int ALIGN_SPEED = 150;              // PWM while turning to the target
const bool ALIGN_ENABLED = true;          // true = rotate the shortest way onto the target, then laser ON.
                                          // false = stop in place, laser ON there (use while geometry is unverified).
const unsigned long MOTION_GRACE_MS = 600;
const unsigned long MOTION_CHECK_MS = 500;
const long STALL_MIN_COUNTS = 3;          // encoder counts that count as "moving"
const float STALL_MIN_GYRO_DEG = 3.0f;    // gyro degrees that count as "moving"
const bool REQUIRE_MOTION_FEEDBACK = true; // fault if neither encoder nor gyro sees rotation

// ---------- Detection / debounce (spin_until_ir approach) ----------
const unsigned long SAMPLE_PERIOD_US = 100;
const unsigned long WINDOW_MS = 20;       // ~200 samples per window
const int DETECT_K = 5;                   // LOW reads per window = signal
const int CONFIRM_WINDOWS = 3;            // same single sensor, consecutive windows
const int REARM_CLEAR_WINDOWS = 10;       // 10 x 20 ms = 200 ms clear before next detection
const unsigned long STUCK_MS = 3000;

// ---------- Cycle ----------
const unsigned long HOLD_MS = 5000;       // hold at the target, laser ON
const unsigned long SCAN_TIMEOUT_MS = 0;  // 0 = scan indefinitely
const int MAX_AUTO_RETRIES = 3;           // automatic restarts after a fault (reset after a good cycle or 'g')
const unsigned long FAULT_RETRY_MS = 5000;
const float ALIGN_LEAD_DEG = 3.0f;        // command stop when this much turn remains (brake lead)
const float ALIGN_ACCEPT_DEG = 5.0f;      // final |error| accepted after settling
const unsigned long ALIGN_TIMEOUT_MS = 8000;
const unsigned long ALIGN_SETTLE_MS = 150;

// ---------- Laser ----------
const bool LASER_ENABLED_AT_BOOT = true;  // master flag; laser is ON only during the HOLD state

// ---------- Telemetry ----------
const unsigned long TELEMETRY_MS = 500;   // 0 = off; one compact line per interval

// ---------- MPU6050 ----------
const uint8_t MPU_ADDR = 0x68;            // AD0 -> GND
const uint8_t REG_WHO_AM_I = 0x75;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_GYRO_XOUT_H = 0x43;
const int GYRO_AXIS = 2;                  // 0 = X, 1 = Y, 2 = Z (axis parallel to the spin axis)
const int GYRO_RANGE_SEL = 2;             // 0 = +/-250, 1 = +/-500, 2 = +/-1000, 3 = +/-2000 deg/s
const float GYRO_LSB[4] = {131.0f, 65.5f, 32.8f, 16.4f};
const unsigned long GYRO_POLL_MS = 10;    // 100 Hz
const int GYRO_BIAS_SAMPLES = 200;        // 200 x 5 ms = 1 s, assembly must be still
const float GYRO_STILL_TOL_DPS = 4.0f;    // max-min spread allowed during bias measurement
const int GYRO_INIT_ATTEMPTS = 10;        // 1 s each: wait for the assembly to stop coasting after a reset
const float GYRO_DEADBAND_DPS = 0.3f;     // ignore |rate - bias| below this (limits drift)
const unsigned long GYRO_REBIAS_SETTLE_MS = 1000; // during HOLD: skip first second, then average
const bool CROSSCHECK_FAULT = true;       // fault when encoder and gyro disagree
const float CROSSCHECK_MIN_DEG = 8.0f;    // minimum tolerated disagreement
const float CROSSCHECK_FRACTION = 0.15f;  // or this fraction of the movement, whichever is larger

// ---------- State ----------
enum State { ST_ROTATING, ST_ALIGN, ST_HOLD, ST_PAUSED, ST_FAULT };
enum Fault { F_NONE, F_NO_MOTION, F_SENSOR_STUCK, F_ALIGN_FAIL, F_POS_MISMATCH, F_TIMEOUT };

State state = ST_PAUSED;      // laser stays OFF until setup() finishes
Fault fault = F_NONE;
unsigned long stateSinceMs = 0;

int scanDir = 1;              // +1 = DIR HIGH (clockwise), -1 = DIR LOW
bool laserMaster = LASER_ENABLED_AT_BOOT;
bool laserOn = false;

volatile long encoderCount = 0;
int rotateSign = 1;           // counts * rotateSign -> angle increasing with DIR=HIGH
bool signLearned = false;

bool gyroOk = false;
int gyroFailRun = 0;
float gyroBiasDps = 0.0f;
float gyroRawDeg = 0.0f;      // integrated, un-signed
float gyroRateDps = 0.0f;     // last bias-corrected rate, un-signed
int gyroSign = 1;
bool gyroSignLearned = false;
unsigned long lastGyroUs = 0;

// hold-time gyro re-bias statistics
float holdSum = 0.0f, holdMin = 0.0f, holdMax = 0.0f;
int holdN = 0;
bool gyroRetryDone = false;   // one gyro re-init attempt per hold (assembly is still then)
int faultRetries = 0;

// sampling window
unsigned long lastSampleUs = 0;
unsigned long windowStartMs = 0;
float windowStartPose = NAN;
uint16_t counts[NUM_SENSORS];
uint16_t samples = 0;

// detection bookkeeping
bool armed = false;
int clearRun = 0;
int confirmRun = 0;
uint16_t runSum[NUM_SENSORS];     // LOW reads summed over the confirming windows
float runFirstPose[NUM_SENSORS]; // pose where each sensor first reached K in this run
bool runFirstSet[NUM_SENSORS];
unsigned long lowRunMs[NUM_SENSORS];
float candidateStartPose = NAN;

// motion check
unsigned long rotateStartMs = 0;
unsigned long motionArmedAtMs = 0;
bool motionRefValid = false;
unsigned long motionRefMs = 0;
long motionRefCounts = 0;
float motionRefGyroRaw = 0.0f;
int mismatchRun = 0;

// detection / alignment
int detSensor = -1;
int detScanDir = 1;
float detStopPose = NAN;
float targetBearingDeg = NAN;
float angleErrorDeg = 0.0f;
float alignStartPose = NAN;
long alignStartCounts = 0;
float alignStartGyroRaw = 0.0f;
bool alignSettling = false;
unsigned long alignSettleStartMs = 0;
float alignRemainingDeg = 0.0f;

// hold
unsigned long holdStartMs = 0;
float holdStartPose = NAN;

// motor + telemetry bookkeeping
bool motorPositive = true;
int motorPwm = 0;
int lastWinBest = -1;          // strongest sensor in the last completed window
uint16_t lastWinBestCount = 0;
uint16_t lastWinSamples = 0;
bool haveDetection = false;    // a target has been confirmed since boot
char telemBuf[256];
uint16_t telemLen = 0, telemPos = 0;
unsigned long lastTelemMs = 0;

void resetConfirm();

// ---------- Angle helpers ----------
float wrap360(float d) {
  d = fmodf(d, 360.0f);
  if (d < 0) d += 360.0f;
  return d;
}

float wrap180(float d) {
  d = wrap360(d);
  if (d > 180.0f) d -= 360.0f;
  return d;
}

void printAngle(float d) {
  if (isnan(d)) {
    Serial.print(F("n/a"));
  } else {
    Serial.print(d, 1);
  }
}

// ---------- Encoder ----------
void encoderISR() {
  if (digitalRead(ENCODER_B_PIN)) {
    encoderCount++;
  } else {
    encoderCount--;
  }
}

long readCounts() {
  noInterrupts();
  long c = encoderCount;
  interrupts();
  return c;
}

float countsToDeg(long c) {
  if (COUNTS_PER_REV <= 0) return NAN;
  return (float)rotateSign * (float)c * 360.0f / (float)COUNTS_PER_REV;
}

// ---------- MPU6050 ----------
bool mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool mpuReadReg(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

bool mpuReadGyroDps(float &dps) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write((uint8_t)(REG_GYRO_XOUT_H + 2 * GYRO_AXIS));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)2) != 2) return false;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  int16_t raw = (int16_t)(((uint16_t)hi << 8) | lo);
  dps = (float)raw / GYRO_LSB[GYRO_RANGE_SEL];
  return true;
}

// Full burst read for telemetry: accel X/Y/Z (g, +/-2 g default), temp (C), gyro X/Y/Z (deg/s, raw).
bool mpuReadAll(float a[3], float g[3], float &tempC) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write((uint8_t)0x3B); // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)14) != 14) return false;
  int16_t v[7];
  for (int i = 0; i < 7; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    v[i] = (int16_t)(((uint16_t)hi << 8) | lo);
  }
  for (int i = 0; i < 3; i++) {
    a[i] = (float)v[i] / 16384.0f;
    g[i] = (float)v[4 + i] / GYRO_LSB[GYRO_RANGE_SEL];
  }
  tempC = (float)v[3] / 340.0f + 36.53f;
  return true;
}

// Waits (up to `attempts` x 1 s) for the assembly to be still, e.g. after a reset while it was coasting.
bool gyroCalibrateBias(int attempts) {
  for (int a = 0; a < attempts; a++) {
    float sum = 0.0f, mn = 1e9f, mx = -1e9f;
    for (int i = 0; i < GYRO_BIAS_SAMPLES; i++) {
      float dps;
      if (!mpuReadGyroDps(dps)) return false;
      sum += dps;
      if (dps < mn) mn = dps;
      if (dps > mx) mx = dps;
      delay(5);
    }
    if (mx - mn <= GYRO_STILL_TOL_DPS) {
      gyroBiasDps = sum / GYRO_BIAS_SAMPLES;
      Serial.print(F("MPU6050 gyro bias = "));
      Serial.print(gyroBiasDps, 3);
      Serial.println(F(" dps"));
      return true;
    }
    Serial.print(F("MPU6050: assembly not still (spread "));
    Serial.print(mx - mn, 1);
    Serial.print(F(" dps), waiting "));
    Serial.print(a + 1);
    Serial.print('/');
    Serial.println(attempts);
  }
  Serial.println(F("MPU6050: bias calibration failed - keep the assembly and board still"));
  return false;
}

bool mpuInit(int attempts) {
  Wire.begin();
  Wire.setClock(400000);
#ifdef WIRE_HAS_TIMEOUT
  Wire.setWireTimeout(3000, true);
#endif

  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println(F("MPU6050: no ACK at 0x68 (check SDA=A4, SCL=A5, AD0=GND, power)"));
    return false;
  }
  uint8_t who = 0;
  if (!mpuReadReg(REG_WHO_AM_I, who) || who != 0x68) {
    Serial.print(F("MPU6050: WHO_AM_I = 0x"));
    Serial.print(who, HEX);
    Serial.println(F(" (expected 0x68)"));
    return false;
  }
  if (!mpuWriteReg(REG_PWR_MGMT_1, 0x00)) return false; // wake from sleep
  delay(100);
  if (!mpuWriteReg(REG_GYRO_CONFIG, (uint8_t)(GYRO_RANGE_SEL << 3))) return false;
  uint8_t cfg = 0xFF;
  if (!mpuReadReg(REG_GYRO_CONFIG, cfg) || cfg != (uint8_t)(GYRO_RANGE_SEL << 3)) return false;
  delay(50);

  if (!gyroCalibrateBias(attempts)) return false;
  gyroFailRun = 0;
  Serial.println(F("MPU6050 verified at 0x68 - gyro logic ENABLED"));
  return true;
}

void gyroPoll(bool force) {
  if (!gyroOk) return;
  unsigned long nowUs = micros();
  if (!force && nowUs - lastGyroUs < GYRO_POLL_MS * 1000UL) return;

  float dps;
  if (!mpuReadGyroDps(dps)) {
    if (++gyroFailRun >= 5) {
      gyroOk = false;
      Serial.println(F("MPU6050 read failed - gyro logic DISABLED"));
    }
    return;
  }
  gyroFailRun = 0;

  float dt = (float)(nowUs - lastGyroUs) * 1e-6f;
  lastGyroUs = nowUs;

  float r = dps - gyroBiasDps;
  gyroRateDps = r;
  if (fabsf(r) >= GYRO_DEADBAND_DPS) gyroRawDeg += r * dt;

  if (state == ST_HOLD && millis() - stateSinceMs >= GYRO_REBIAS_SETTLE_MS) {
    if (holdN == 0) { holdMin = dps; holdMax = dps; }
    holdSum += dps;
    if (dps < holdMin) holdMin = dps;
    if (dps > holdMax) holdMax = dps;
    holdN++;
  }
}

float gyroDegNow() {
  return (float)gyroSign * gyroRawDeg;
}

// ---------- Pose (angle in the DIR=HIGH-positive sense) ----------
// Encoder is the primary source when calibrated; otherwise the verified gyro.
bool poseAvailable() {
  return COUNTS_PER_REV > 0 || gyroOk;
}

float poseDeg() {
  if (COUNTS_PER_REV > 0) return countsToDeg(readCounts());
  if (gyroOk) return gyroDegNow();
  return NAN;
}

// Detection is only armed once the sign of the active angle source is known,
// so the first bearing is never computed with a guessed direction.
bool poseSignsReady() {
  if (COUNTS_PER_REV > 0) return signLearned;
  if (gyroOk) return gyroSignLearned;
  return true;
}

const __FlashStringHelper *poseSource() {
  if (COUNTS_PER_REV > 0) return F("encoder");
  if (gyroOk) return F("gyro");
  return F("none");
}

// ---------- Laser / motor ----------
// Laser rule: ON only in HOLD (confirmed target, motor stopped). OFF while scanning,
// while turning to the target, when paused, in a fault and at boot.
void applyLaser() {
  bool want = laserMaster && (state == ST_HOLD);
  if (want != laserOn) {
    digitalWrite(LASER_PIN, want ? HIGH : LOW);
    laserOn = want;
  }
}

void setState(State s) {
  state = s;
  stateSinceMs = millis();
  applyLaser();
}

void motorRun(bool positive, int speed) {
  digitalWrite(MOTOR_DIR_PIN, positive ? HIGH : LOW);
  analogWrite(MOTOR_PWM_PIN, speed);
  motorPositive = positive;
  motorPwm = speed;
}

void motorStop() {
  analogWrite(MOTOR_PWM_PIN, 0); // PH/EN mode: EN low brakes the motor
  motorPwm = 0;
}

void resetWindow(unsigned long now) {
  for (int i = 0; i < NUM_SENSORS; i++) counts[i] = 0;
  samples = 0;
  windowStartMs = now;
  windowStartPose = poseDeg();
}

const __FlashStringHelper *faultName(Fault f) {
  switch (f) {
    case F_NO_MOTION:    return F("NO_MOTION (neither encoder nor gyro sees rotation)");
    case F_SENSOR_STUCK: return F("SENSOR_STUCK (sensor solid LOW)");
    case F_ALIGN_FAIL:   return F("ALIGN_FAIL (rotation to target failed)");
    case F_POS_MISMATCH: return F("POS_MISMATCH (encoder and gyro disagree)");
    case F_TIMEOUT:      return F("TIMEOUT (no detection)");
    default:             return F("NONE");
  }
}

void enterFault(Fault f) {
  motorStop();
  digitalWrite(MOTOR_SLEEP_PIN, LOW);
  fault = f;
  setState(ST_FAULT); // laser goes OFF here
  Serial.print(F("!!! FAULT: "));
  Serial.println(faultName(f));
  if (faultRetries < MAX_AUTO_RETRIES) {
    Serial.print(F("auto-restart in "));
    Serial.print(FAULT_RETRY_MS / 1000);
    Serial.println(F(" s (or send 'g')"));
  } else {
    Serial.println(F("retries used up - send 'g' to restart"));
  }
}

void enterRotating() {
  unsigned long now = millis();
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);
  fault = F_NONE;
  armed = false;
  clearRun = 0;
  resetConfirm();
  mismatchRun = 0;
  for (int i = 0; i < NUM_SENSORS; i++) lowRunMs[i] = 0;
  rotateStartMs = now;
  motionRefValid = false;
  motionArmedAtMs = now + MOTION_GRACE_MS;
  setState(ST_ROTATING);
  resetWindow(now);
  motorRun(scanDir > 0, SPIN_SPEED);
  Serial.print(F(">>> SCANNING ("));
  Serial.print(scanDir > 0 ? F("clockwise") : F("anti-clockwise"));
  Serial.println(F(", laser OFF) <<<"));
}

void enterPaused() {
  motorStop();
  setState(ST_PAUSED);
  Serial.println(F(">>> STOPPED <<<"));
}

void enterHold() {
  motorStop();
  holdStartMs = millis();
  holdStartPose = poseDeg();
  holdSum = 0.0f;
  holdN = 0;
  gyroRetryDone = false;
  setState(ST_HOLD);
  Serial.print(F(">>> HOLD "));
  Serial.print(HOLD_MS / 1000.0f, 1);
  Serial.println(F(" s - laser ON, marking target <<<"));
}

// Target bearing from the fixed centre. Scanning in direction scanDir, a sensor first
// reaches K when the target is EDGE_HALF_DEG ahead of its axis in that direction:
//   bearing = theta_first_seen + mount + scanDir * alpha
float estimateBearingDeg(int sensor, float firstSeenPose, int dir) {
  float alpha = EDGE_HALF_DEG;
  if (SENSOR_RADIUS_MM > 0 && TARGET_DISTANCE_MM > 0) {
    float a = EDGE_HALF_DEG * DEG_TO_RAD;
    alpha = atan2f(TARGET_DISTANCE_MM * sinf(a),
                   SENSOR_RADIUS_MM + TARGET_DISTANCE_MM * cosf(a)) * RAD_TO_DEG;
  }
  return wrap360(firstSeenPose + SENSOR_MOUNT_DEG[sensor] + (float)dir * alpha);
}

void beginAlignOrHold() {
  alignSettling = false;

  if (isnan(targetBearingDeg) || isnan(detStopPose)) {
    Serial.println(F("No angle source (encoder not calibrated and gyro not verified): hold in place"));
    enterHold();
    return;
  }

  // shortest-path rotation: angle_error normalised to +/-180 deg, positive = DIR HIGH
  angleErrorDeg = wrap180(targetBearingDeg - (detStopPose + LASER_MOUNT_DEG));
  Serial.print(F("target bearing="));
  printAngle(targetBearingDeg);
  Serial.print(F(" deg, angle_error="));
  printAngle(angleErrorDeg);
  Serial.println(angleErrorDeg > 0 ? F(" deg (clockwise)") : F(" deg (anti-clockwise)"));

  if (!ALIGN_ENABLED) {
    Serial.println(F("ALIGN_ENABLED = false: not rotating, holding in place"));
    enterHold();
    return;
  }
  if (fabsf(angleErrorDeg) <= ALIGN_ACCEPT_DEG) {
    enterHold();
    return;
  }

  alignStartPose = poseDeg();
  alignStartCounts = readCounts();
  alignStartGyroRaw = gyroRawDeg;
  setState(ST_ALIGN);
  motorRun(angleErrorDeg > 0, ALIGN_SPEED);
  Serial.println(F(">>> ROTATING TO TARGET <<<"));
}

void acceptDetection(int sensor) {
  motorStop();          // stop first, before any maths or printing
  gyroPoll(true);       // fresh gyro angle at the stop instant
  detSensor = sensor;
  haveDetection = true;
  detScanDir = scanDir;
  long stopCounts = readCounts();
  detStopPose = poseDeg();
  targetBearingDeg = isnan(candidateStartPose) ? NAN
                     : estimateBearingDeg(sensor, candidateStartPose, detScanDir);

  Serial.print(F("DETECT "));
  Serial.print(SENSOR_NAME[sensor]);
  Serial.print(F(" mount="));
  Serial.print(SENSOR_MOUNT_DEG[sensor], 0);
  Serial.print(F(" enc="));
  Serial.print(stopCounts);
  Serial.print(F("cnt("));
  printAngle(countsToDeg(stopCounts));
  Serial.print(F("deg) gyro="));
  printAngle(gyroOk ? gyroDegNow() : NAN);
  Serial.print(F("deg src="));
  Serial.print(poseSource());
  Serial.print(F(" first_seen="));
  printAngle(candidateStartPose);
  Serial.print(F(" stop="));
  printAngle(detStopPose);
  Serial.println();

  beginAlignOrHold();
}

// ---------- Per-window evaluation ----------
void resetConfirm() {
  confirmRun = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    runSum[i] = 0;
    runFirstSet[i] = false;
  }
}

void evaluateWindow() {
  int nHit = 0;

  lastWinBest = 0;
  for (int i = 1; i < NUM_SENSORS; i++) {
    if (counts[i] > counts[lastWinBest]) lastWinBest = i;
  }
  lastWinBestCount = counts[lastWinBest];
  lastWinSamples = samples;

  for (int i = 0; i < NUM_SENSORS; i++) {
    if (counts[i] >= DETECT_K) nHit++;

    if (samples > 0 && (unsigned long)counts[i] * 20UL >= (unsigned long)samples * 19UL) {
      lowRunMs[i] += WINDOW_MS;
      if (lowRunMs[i] >= STUCK_MS) { enterFault(F_SENSOR_STUCK); return; }
    } else {
      lowRunMs[i] = 0;
    }
  }

  if (!armed) {
    clearRun = (nHit == 0) ? clearRun + 1 : 0;
    if (clearRun >= REARM_CLEAR_WINDOWS && poseSignsReady()) {
      armed = true;
      Serial.println(F("armed"));
    }
    return;
  }

  // No sensor in signal: the run is broken
  if (nHit == 0) {
    resetConfirm();
    return;
  }

  // One or several sensors in signal (a beacon can reach neighbouring sensors at once):
  // keep a run of consecutive windows, remember where each sensor first reached K,
  // and let the sensor with the most LOW reads over the run win.
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (counts[i] >= DETECT_K) {
      if (!runFirstSet[i]) {
        runFirstSet[i] = true;
        runFirstPose[i] = windowStartPose;
      }
      runSum[i] += counts[i];
    }
  }

  if (++confirmRun >= CONFIRM_WINDOWS) {
    int winner = 0;
    for (int i = 1; i < NUM_SENSORS; i++) {
      if (runSum[i] > runSum[winner]) winner = i;
    }
    candidateStartPose = runFirstPose[winner];
    acceptDetection(winner);
  }
}

// Verifies rotation with encoder and gyro, learns their signs, cross-checks them.
void checkMotion(unsigned long now) {
  if ((long)(now - motionArmedAtMs) < 0) return;
  if (!motionRefValid) {
    motionRefValid = true;
    motionRefMs = now;
    motionRefCounts = readCounts();
    motionRefGyroRaw = gyroRawDeg;
    return;
  }
  if (now - motionRefMs < MOTION_CHECK_MS) return;

  long c = readCounts();
  long dEnc = c - motionRefCounts;
  float dGyroRaw = gyroRawDeg - motionRefGyroRaw;
  bool encMoved = labs(dEnc) >= STALL_MIN_COUNTS;
  bool gyroMoved = gyroOk && fabsf(dGyroRaw) >= STALL_MIN_GYRO_DEG;

  if (REQUIRE_MOTION_FEEDBACK && !encMoved && !gyroMoved) {
    enterFault(F_NO_MOTION);
    return;
  }

  // gyro sees rotation but the calibrated encoder does not count: encoder wiring/CPR problem
  if (COUNTS_PER_REV > 0 && !encMoved && gyroMoved) {
    Serial.println(F("XCHECK encoder not counting while gyro sees rotation"));
    if (CROSSCHECK_FAULT) {
      enterFault(F_POS_MISMATCH);
      return;
    }
  }

  if (encMoved && !signLearned) {
    rotateSign = ((dEnc > 0) == (scanDir > 0)) ? 1 : -1;
    signLearned = true;
    Serial.print(F("encoder sign learned: "));
    Serial.println(rotateSign);
  }
  if (gyroMoved && !gyroSignLearned && fabsf(dGyroRaw) >= 5.0f) {
    gyroSign = ((dGyroRaw > 0) == (scanDir > 0)) ? 1 : -1;
    gyroSignLearned = true;
    Serial.print(F("gyro sign learned: "));
    Serial.println(gyroSign);
  }

  if (encMoved && gyroMoved && signLearned && gyroSignLearned && COUNTS_PER_REV > 0) {
    float dEncDeg = countsToDeg(dEnc);
    float dGyroDeg = (float)gyroSign * dGyroRaw;
    float err = fabsf(dEncDeg - dGyroDeg);
    float tol = fmaxf(CROSSCHECK_MIN_DEG, CROSSCHECK_FRACTION * fabsf(dEncDeg));
    if (err > tol) {
      Serial.print(F("XCHECK encoder "));
      Serial.print(dEncDeg, 1);
      Serial.print(F(" deg vs gyro "));
      Serial.print(dGyroDeg, 1);
      Serial.println(F(" deg"));
      if (++mismatchRun >= 3 && CROSSCHECK_FAULT) {
        enterFault(F_POS_MISMATCH);
        return;
      }
    } else {
      mismatchRun = 0;
    }
  }

  motionRefMs = now;
  motionRefCounts = c;
  motionRefGyroRaw = gyroRawDeg;
}

void printStatus() {
  long c = readCounts();
  Serial.print(F("state="));
  Serial.print((int)state);
  Serial.print(F(" enc="));
  Serial.print(c);
  Serial.print(F("cnt("));
  printAngle(countsToDeg(c));
  Serial.print(F("deg) gyro="));
  printAngle(gyroOk ? gyroDegNow() : NAN);
  Serial.print(F("deg rate="));
  Serial.print(gyroSign * gyroRateDps, 1);
  Serial.print(F("dps src="));
  Serial.print(poseSource());
  Serial.print(F(" laser="));
  Serial.print(laserOn ? 1 : 0);
  Serial.print(F(" armed="));
  Serial.print(armed ? 1 : 0);
  if (gyroOk && fabsf(gyroRawDeg) >= 360.0f) {
    Serial.print(F(" CPR_est(from gyro)="));
    Serial.print(fabsf((float)c) * 360.0f / fabsf(gyroRawDeg), 0);
  }
  Serial.print(F(" fault="));
  Serial.println(faultName(fault));
}

// ---------- Telemetry (non-blocking: the line is built in a buffer and fed to the UART as space frees up) ----------
char *tw;
char *tend;

void putS(const char *s) {
  size_t n = strlen(s);
  if (tw + n > tend) return;
  memcpy(tw, s, n);
  tw += n;
}

void putP(const __FlashStringHelper *f) {
  size_t n = strlen_P((PGM_P)f);
  if (tw + n > tend) return;
  memcpy_P(tw, f, n);
  tw += n;
}

void putF(float v, int prec, bool plus = false) {
  if (isnan(v) || fabsf(v) > 99999.0f) { putP(F("n/a")); return; }
  char tmp[16];
  if (plus && v >= 0) putS("+");
  dtostrf(v, 1, prec, tmp);
  putS(tmp);
}

void putL(long v) {
  char tmp[14];
  ltoa(v, tmp, 10);
  putS(tmp);
}

float shown(float d) {
  return isnan(d) ? d : wrap360(d);
}

const __FlashStringHelper *faultShort(Fault f) {
  switch (f) {
    case F_NO_MOTION:    return F("NO_MOTION");
    case F_SENSOR_STUCK: return F("SENSOR_STUCK");
    case F_ALIGN_FAIL:   return F("ALIGN_FAIL");
    case F_POS_MISMATCH: return F("POS_MISMATCH");
    case F_TIMEOUT:      return F("TIMEOUT");
    default:             return F("NONE");
  }
}

// One line, e.g.
// Acc(g) X:0.04 Y:-0.19 Z:0.97 | Gyro(dps) X:-1.5 Y:0.4 Z:-1.4 | T:26.6C | Enc:1234cnt 45.2deg | GyroAng:44.8deg |
// Sig:S2 7/200lo conf2/3 | Det:S2 Brg:155.0deg LaserAx:0.0deg Err:+141.0deg | Mot:CW pwm200 | Laser:OFF | SCAN armed
void buildTelemetry(unsigned long now) {
  tw = telemBuf;
  tend = telemBuf + sizeof(telemBuf) - 2;

  float a[3], g[3], tC;
  if (gyroOk && mpuReadAll(a, g, tC)) {
    putP(F("Acc(g) X:")); putF(a[0], 2);
    putP(F(" Y:"));       putF(a[1], 2);
    putP(F(" Z:"));       putF(a[2], 2);
    putP(F(" | Gyro(dps) X:")); putF(g[0], 1);
    putP(F(" Y:"));             putF(g[1], 1);
    putP(F(" Z:"));             putF(g[2], 1);
    putP(F(" | T:")); putF(tC, 1); putP(F("C"));
  } else {
    putP(F("IMU:OFF"));
  }

  long c = readCounts();
  putP(F(" | Enc:")); putL(c); putP(F("cnt ")); putF(shown(countsToDeg(c)), 1); putP(F("deg"));
  putP(F(" | GyroAng:")); putF(gyroOk ? shown(gyroDegNow()) : NAN, 1); putP(F("deg"));

  putP(F(" | Sig:"));
  if (lastWinBest >= 0 && lastWinBestCount > 0) { putS("S"); putL(lastWinBest + 1); } else putP(F("--"));
  putS(" "); putL(lastWinBestCount); putS("/"); putL(lastWinSamples);
  putP(F("lo conf")); putL(state == ST_ROTATING ? confirmRun : 0); putS("/"); putL(CONFIRM_WINDOWS);

  float pose = poseDeg();
  bool live = (state == ST_ALIGN || state == ST_HOLD) && !isnan(targetBearingDeg) && !isnan(pose);
  putP(F(" | Det:"));
  if (haveDetection) { putS("S"); putL(detSensor + 1); } else putP(F("--"));
  putP(F(" Brg:"));     putF(targetBearingDeg, 1); putP(F("deg"));
  putP(F(" LaserAx:")); putF(isnan(pose) ? NAN : wrap360(pose + LASER_MOUNT_DEG), 1); putP(F("deg"));
  putP(F(" Err:"));
  if (live) { putF(wrap180(targetBearingDeg - (pose + LASER_MOUNT_DEG)), 1, true); putP(F("deg")); }
  else putP(F("--"));

  putP(F(" | Mot:"));
  if (motorPwm == 0) putP(F("OFF"));
  else { putS(motorPositive ? "CW" : "CCW"); putP(F(" pwm")); putL(motorPwm); }
  putP(laserOn ? F(" | Laser:ON") : F(" | Laser:OFF"));

  putP(F(" | "));
  switch (state) {
    case ST_ROTATING: putP(F("SCAN ")); putP(armed ? F("armed") : F("arming")); break;
    case ST_ALIGN:    putP(F("ALIGN rem:")); putF(alignRemainingDeg, 1, true); putP(F("deg")); break;
    case ST_HOLD: {
      long left = (long)HOLD_MS - (long)(now - holdStartMs);
      putP(F("HOLD ")); putF((left > 0 ? left : 0) / 1000.0f, 1); putP(F("s left"));
      break;
    }
    case ST_PAUSED:   putP(F("STOP")); break;
    case ST_FAULT:    putP(F("FAULT ")); putP(faultShort(fault)); break;
  }
  putS("\n");
  telemLen = tw - telemBuf;
  telemPos = 0;
}

void pumpTelemetry(unsigned long now) {
  while (telemPos < telemLen && Serial.availableForWrite() > 0) {
    Serial.write(telemBuf[telemPos++]);
  }
  if (TELEMETRY_MS > 0 && telemPos >= telemLen && now - lastTelemMs >= TELEMETRY_MS) {
    lastTelemMs = now;
    buildTelemetry(now);
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'g' || c == 'G') {
      faultRetries = 0;
      if (state != ST_ROTATING) enterRotating();
    } else if (c == ' ') {
      if (state != ST_PAUSED) enterPaused();
    } else if (c == 'r' || c == 'R') {
      scanDir = -scanDir;
      enterRotating();
    } else if (c == 'z') {
      noInterrupts();
      encoderCount = 0;
      interrupts();
      gyroRawDeg = 0.0f;
      Serial.println(F("position zeroed (encoder + gyro)"));
    } else if (c == 'p') {
      printStatus();
    } else if (c == 'l') {
      laserMaster = !laserMaster;
      applyLaser();
      Serial.println(laserMaster ? F("laser master ON") : F("laser master OFF"));
    }
  }
}

// ---------- Arduino entry points ----------
void setup() {
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);        // laser explicitly OFF first
  laserOn = false;

  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);

  for (int i = 0; i < NUM_SENSORS; i++) pinMode(SENSOR_PINS[i], INPUT_PULLUP);
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);

  Serial.println(F("=== Spin Detect v0.9 (S1=D4@0, S2=D5@120, S3=D6@240, laser=D10, MPU6050=A4/A5) ==="));
  Serial.println(F("Keep the assembly still: measuring gyro bias..."));
  gyroOk = mpuInit(GYRO_INIT_ATTEMPTS);
  lastGyroUs = micros();
  if (!gyroOk) {
    Serial.println(F("Gyro logic DISABLED - using encoder only"));
  }
  if (COUNTS_PER_REV <= 0) {
    Serial.println(gyroOk ? F("COUNTS_PER_REV = 0: gyro is the angle source")
                          : F("WARNING: no angle source - bearing/alignment disabled"));
  }
  if (!ALIGN_ENABLED) {
    Serial.println(F("NOTE: ALIGN_ENABLED = false - stop in place, no rotation to target"));
  }
  Serial.println(F("g start | space stop | r reverse | z zero | p status | l laser"));
  delay(500);

  enterRotating();                     // auto-start like motor_onoff; laser turns ON here
}

void loop() {
  unsigned long now = millis();

  handleSerial();
  gyroPoll(false);
  applyLaser();
  pumpTelemetry(now);

  switch (state) {
    case ST_ROTATING: {
      unsigned long nowUs = micros();
      if (nowUs - lastSampleUs >= SAMPLE_PERIOD_US) {
        lastSampleUs = nowUs;
        for (int i = 0; i < NUM_SENSORS; i++) {
          if (digitalRead(SENSOR_PINS[i]) == LOW) counts[i]++;
        }
        samples++;
      }

      if (now - windowStartMs >= WINDOW_MS) {
        evaluateWindow();
        if (state == ST_ROTATING) resetWindow(now);
      }
      if (state != ST_ROTATING) break;

      checkMotion(now);
      if (state != ST_ROTATING) break;

      if (SCAN_TIMEOUT_MS > 0 && now - rotateStartMs > SCAN_TIMEOUT_MS) {
        enterFault(F_TIMEOUT);
        break;
      }

      break;
    }

    case ST_ALIGN: {
      float cur = poseDeg();
      if (isnan(cur)) { enterFault(F_ALIGN_FAIL); break; }
      float moved = cur - alignStartPose;
      float progress = (angleErrorDeg > 0) ? moved : -moved; // + = toward target
      float remaining = fabsf(angleErrorDeg) - progress;
      alignRemainingDeg = (angleErrorDeg > 0) ? remaining : -remaining; // signed, like angle_error

      if (!alignSettling) {
        if (progress < -ALIGN_ACCEPT_DEG || now - stateSinceMs > ALIGN_TIMEOUT_MS) {
          enterFault(F_ALIGN_FAIL);
        } else if (remaining <= ALIGN_LEAD_DEG) {
          motorStop();
          alignSettling = true;
          alignSettleStartMs = now;
        }
      } else if (now - alignSettleStartMs >= ALIGN_SETTLE_MS) {
        gyroPoll(true);
        cur = poseDeg();
        moved = cur - alignStartPose;
        progress = (angleErrorDeg > 0) ? moved : -moved;
        remaining = fabsf(angleErrorDeg) - progress;

        bool ok = fabsf(remaining) <= ALIGN_ACCEPT_DEG;

        // movement verification: encoder and gyro must agree on how far we turned
        if (ok && COUNTS_PER_REV > 0 && gyroOk && signLearned && gyroSignLearned) {
          float encMove = countsToDeg(readCounts() - alignStartCounts);
          float gyroMove = (float)gyroSign * (gyroRawDeg - alignStartGyroRaw);
          float tol = fmaxf(CROSSCHECK_MIN_DEG, CROSSCHECK_FRACTION * fabsf(encMove));
          if (fabsf(encMove - gyroMove) > tol) {
            Serial.print(F("XCHECK align encoder "));
            Serial.print(encMove, 1);
            Serial.print(F(" deg vs gyro "));
            Serial.print(gyroMove, 1);
            Serial.println(F(" deg"));
            if (CROSSCHECK_FAULT) ok = false;
          }
        }

        if (ok) {
          Serial.print(F("aligned, residual error "));
          Serial.print(remaining, 1);
          Serial.println(F(" deg"));
          enterHold();
        } else {
          enterFault(F_ALIGN_FAIL);
        }
      }
      break;
    }

    case ST_HOLD:
      // gyro failed at boot (e.g. the assembly was still coasting): try once while it is still
      if (!gyroOk && !gyroRetryDone && now - holdStartMs >= 1500 && now - holdStartMs < HOLD_MS - 2500) {
        gyroRetryDone = true;
        gyroOk = mpuInit(1);
        lastGyroUs = micros();
      }
      if (now - holdStartMs >= HOLD_MS) {
        Serial.print(F("hold drift = "));
        float p = poseDeg();
        printAngle(isnan(p) || isnan(holdStartPose) ? NAN : p - holdStartPose);
        Serial.println(F(" deg"));
        faultRetries = 0;              // a complete cycle: clear the retry budget
        // assembly has been still: refresh the gyro bias if the data were clean
        if (gyroOk && holdN >= 50 && (holdMax - holdMin) <= GYRO_STILL_TOL_DPS) {
          gyroBiasDps = holdSum / holdN;
          Serial.print(F("gyro bias updated: "));
          Serial.println(gyroBiasDps, 3);
        }
        enterRotating();
      }
      break;

    case ST_PAUSED:
    case ST_FAULT:
      if (faultRetries < MAX_AUTO_RETRIES && now - stateSinceMs >= FAULT_RETRY_MS) {
        faultRetries++;
        Serial.print(F("auto-restart attempt "));
        Serial.println(faultRetries);
        enterRotating();
      }
      break;
  }
}
