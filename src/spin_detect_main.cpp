// ============================================================
// SPIN DETECT (v0.7) - Nano V3
// 3x TSOP4138 @ 120 deg + encoder + MPU6050 gyro + red laser
// Build/upload with: pio run -e spin_detect -t upload
// Then:              pio device monitor -e spin_detect
//
// Base: motor_onoff (auto-start, g / space / r, DIR+PWM+nSLEEP pins)
//       spin_until_ir (window count >= K, clear-before-resume).
//
// Sequence:
//   continuous spin (laser ON) -> strong message -> confirm (debounce)
//   -> motor stops -> record encoder, gyro, sensor -> bearing
//   -> rotate the SHORTEST way to the target (angle_error in +/-180)
//   -> stop -> laser stays ON -> hold HOLD_MS (5 s) -> resume spin
//   -> repeat.
//
// Layout: S1 = D4 @ 0 deg (reference, laser axis), S2 = D5 @ 120,
// S3 = D6 @ 240. Angles increase in the DIR=HIGH (clockwise) direction.
// MPU6050: VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68).
// Gyro logic is enabled only after the MPU6050 ACKs at 0x68, WHO_AM_I
// reads 0x68 and the gyro bias calibration passes.
//
// All geometry/calibration values below are CONFIGURABLE PLACEHOLDERS.
// Set ALIGN_ENABLED = false to log bearings without moving to the target.
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
const bool ALIGN_ENABLED = true;          // false = detect + log bearing, stop in place, hold
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
const int MAX_MULTI_WINDOWS = 10;
const unsigned long STUCK_MS = 3000;

// ---------- Cycle ----------
const unsigned long HOLD_MS = 5000;       // hold at the target, laser ON
const unsigned long SCAN_TIMEOUT_MS = 0;  // 0 = scan indefinitely
const float ALIGN_LEAD_DEG = 3.0f;        // command stop when this much turn remains (brake lead)
const float ALIGN_ACCEPT_DEG = 5.0f;      // final |error| accepted after settling
const unsigned long ALIGN_TIMEOUT_MS = 8000;
const unsigned long ALIGN_SETTLE_MS = 150;

// ---------- Laser ----------
const bool LASER_ENABLED_AT_BOOT = true;  // laser is ON while scanning/aligning/holding

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
const float GYRO_DEADBAND_DPS = 0.3f;     // ignore |rate - bias| below this (limits drift)
const unsigned long GYRO_REBIAS_SETTLE_MS = 1000; // during HOLD: skip first second, then average
const bool CROSSCHECK_FAULT = true;       // fault when encoder and gyro disagree
const float CROSSCHECK_MIN_DEG = 8.0f;    // minimum tolerated disagreement
const float CROSSCHECK_FRACTION = 0.15f;  // or this fraction of the movement, whichever is larger

// ---------- State ----------
enum State { ST_ROTATING, ST_ALIGN, ST_HOLD, ST_PAUSED, ST_FAULT };
enum Fault { F_NONE, F_NO_MOTION, F_SENSOR_STUCK, F_MULTI_SENSOR, F_ALIGN_FAIL, F_POS_MISMATCH, F_TIMEOUT };

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

// sampling window
unsigned long lastSampleUs = 0;
unsigned long windowStartMs = 0;
float windowStartPose = NAN;
uint16_t counts[NUM_SENSORS];
uint16_t samples = 0;

// detection bookkeeping
bool armed = false;
int clearRun = 0;
int candidate = -1;
int confirmRun = 0;
int multiRun = 0;
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
unsigned long lastStatusMs = 0;

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

// hold
unsigned long holdStartMs = 0;
float holdStartPose = NAN;

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

bool gyroCalibrateBias() {
  float sum = 0.0f, mn = 1e9f, mx = -1e9f;
  for (int i = 0; i < GYRO_BIAS_SAMPLES; i++) {
    float dps;
    if (!mpuReadGyroDps(dps)) return false;
    sum += dps;
    if (dps < mn) mn = dps;
    if (dps > mx) mx = dps;
    delay(5);
  }
  if (mx - mn > GYRO_STILL_TOL_DPS) {
    Serial.print(F("MPU6050 bias calibration rejected: assembly moving (spread "));
    Serial.print(mx - mn, 2);
    Serial.println(F(" dps)"));
    return false;
  }
  gyroBiasDps = sum / GYRO_BIAS_SAMPLES;
  Serial.print(F("MPU6050 gyro bias = "));
  Serial.print(gyroBiasDps, 3);
  Serial.println(F(" dps"));
  return true;
}

bool mpuInit() {
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

  if (!gyroCalibrateBias()) return false;
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
void applyLaser() {
  bool want = laserMaster && (state == ST_ROTATING || state == ST_ALIGN || state == ST_HOLD);
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
}

void motorStop() {
  analogWrite(MOTOR_PWM_PIN, 0); // PH/EN mode: EN low brakes the motor
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
    case F_MULTI_SENSOR: return F("MULTI_SENSOR (several sensors at once)");
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
  Serial.println(F("Send 'g' to restart."));
}

void enterRotating() {
  unsigned long now = millis();
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);
  fault = F_NONE;
  armed = false;
  clearRun = 0;
  candidate = -1;
  confirmRun = 0;
  multiRun = 0;
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
  Serial.println(F(", laser ON) <<<"));
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
  setState(ST_HOLD);
  Serial.print(F(">>> HOLD "));
  Serial.print(HOLD_MS / 1000.0f, 1);
  Serial.println(F(" s - laser ON <<<"));
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
void evaluateWindow() {
  int nHit = 0, idx = -1;

  for (int i = 0; i < NUM_SENSORS; i++) {
    if (counts[i] >= DETECT_K) { nHit++; idx = i; }

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

  if (nHit >= 2) {
    candidate = -1;
    confirmRun = 0;
    if (++multiRun >= MAX_MULTI_WINDOWS) enterFault(F_MULTI_SENSOR);
    return;
  }
  multiRun = 0;

  if (nHit == 0) {
    candidate = -1;
    confirmRun = 0;
    return;
  }

  if (idx == candidate) {
    confirmRun++;
  } else {
    candidate = idx;
    confirmRun = 1;
    candidateStartPose = windowStartPose; // where this sensor first reached K
  }

  if (confirmRun >= CONFIRM_WINDOWS) {
    acceptDetection(idx);
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

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'g' || c == 'G') {
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

  Serial.println(F("=== Spin Detect v0.7 (S1=D4@0, S2=D5@120, S3=D6@240, laser=D10, MPU6050=A4/A5) ==="));
  Serial.println(F("Keep the assembly still: measuring gyro bias..."));
  gyroOk = mpuInit();
  lastGyroUs = micros();
  if (!gyroOk) {
    Serial.println(F("Gyro logic DISABLED - using encoder only"));
  }
  if (COUNTS_PER_REV <= 0) {
    Serial.println(gyroOk ? F("COUNTS_PER_REV = 0: gyro is the angle source")
                          : F("WARNING: no angle source - bearing/alignment disabled"));
  }
  if (!ALIGN_ENABLED) {
    Serial.println(F("NOTE: ALIGN_ENABLED = false - bearings are logged, no rotation to target"));
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

      if (now - lastStatusMs >= 1000) {
        lastStatusMs = now;
        Serial.print(F("Status: SCANNING enc="));
        Serial.print(readCounts());
        Serial.print(F(" gyro="));
        printAngle(gyroOk ? gyroDegNow() : NAN);
        Serial.print(F(" rate="));
        Serial.print(gyroSign * gyroRateDps, 0);
        Serial.println(F(" dps"));
      }
      break;
    }

    case ST_ALIGN: {
      float cur = poseDeg();
      if (isnan(cur)) { enterFault(F_ALIGN_FAIL); break; }
      float moved = cur - alignStartPose;
      float progress = (angleErrorDeg > 0) ? moved : -moved; // + = toward target
      float remaining = fabsf(angleErrorDeg) - progress;

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
      if (now - holdStartMs >= HOLD_MS) {
        Serial.print(F("hold drift = "));
        float p = poseDeg();
        printAngle(isnan(p) || isnan(holdStartPose) ? NAN : p - holdStartPose);
        Serial.println(F(" deg"));
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
      break;
  }
}
