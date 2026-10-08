// ============================================================
// SPIN DETECT ONE (v0.1) - Nano V3
// single TSOP4138 detection channel + encoder + MPU6050 gyro + one laser
// Build/upload with: pio run -e spin_detect_one -t upload
// Then:              pio device monitor -e spin_detect_one
//
// Standalone and fully automatic: NO serial input or manual command is
// needed (no g / r / space / l). Power up and it runs:
//
//   initialise -> continuous spin (laser OFF) -> strong message
//   -> confirm (spin_until_ir window count + debounce) -> motor stops
//   immediately -> record encoder, gyro, bearing
//   -> [optional] rotate the SHORTEST way onto the target (laser OFF)
//   -> laser ON at the confirmed stopped position -> hold HOLD_MS (5 s)
//   -> laser OFF -> resume spin -> repeat.
//
// The laser is ON only in the HOLD state, never while searching.
// A fault stops the motor and turns the laser OFF; the system retries
// automatically (MAX_AUTO_RETRIES, FAULT_RETRY_MS), then stays safe.
//
// Single sensor on D6 (the IR_CENTRE position used by reactive_ir).
// MPU6050: VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68); gyro logic
// is enabled only after ACK at 0x68, WHO_AM_I = 0x68 and a bias check.
// Telemetry: compact IMU + system state line every TELEMETRY_MS.
//
// All geometry/calibration values are CONFIGURABLE PLACEHOLDERS and
// ALIGN_ENABLED is false until they have been measured and verified.
// Maths, wiring and bench calibration: spin_detect_one.md
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ---------- Pins ----------
const int SENSOR_PIN      = 6;   // TSOP4138, active-LOW
const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_PWM_PIN   = 9;
const int LASER_PIN       = 10;
const int ENCODER_A_PIN   = 2;
const int ENCODER_B_PIN   = 3;
// MPU6050 uses the hardware I2C pins: SDA = A4, SCL = A5 (Wire library)

// ---------- Geometry (intended layout - VERIFY on the bench) ----------
const float SENSOR_MOUNT_DEG = 0.0f;   // sensor axis angle on the assembly
const float LASER_MOUNT_DEG  = 0.0f;   // laser axis angle (0 = co-axial with the sensor)
const float EDGE_HALF_DEG    = 45.0f;  // PLACEHOLDER: off-axis angle where the sensor first reaches K
const float SENSOR_RADIUS_MM   = 0.0f; // 0 = ignore parallax
const float TARGET_DISTANCE_MM = 0.0f; // 0 = ignore parallax
const long  COUNTS_PER_REV = 0;        // encoder counts per assembly turn; 0 = not calibrated
const int   SCAN_DIR = 1;              // +1 = DIR HIGH (clockwise), -1 = DIR LOW

// ---------- Motor ----------
const int SPIN_SPEED  = 200;           // PWM while scanning (spin_until_ir value)
const int ALIGN_SPEED = 150;
const bool ALIGN_ENABLED = false;      // false = stop in place, laser ON there, hold.
                                       // Enable only after COUNTS_PER_REV and geometry are verified.
const unsigned long MOTION_GRACE_MS = 600;
const unsigned long MOTION_CHECK_MS = 500;
const long STALL_MIN_COUNTS = 3;
const float STALL_MIN_GYRO_DEG = 3.0f;
const bool REQUIRE_MOTION_FEEDBACK = true;

// ---------- Detection / debounce (spin_until_ir approach) ----------
const unsigned long SAMPLE_PERIOD_US = 100;
const unsigned long WINDOW_MS = 20;    // ~200 samples per window
const int DETECT_K = 5;                // LOW reads per window = signal
const int CONFIRM_WINDOWS = 3;         // consecutive windows
const int REARM_CLEAR_WINDOWS = 10;    // 10 x 20 ms = 200 ms clear before the next detection
const unsigned long STUCK_MS = 3000;

// ---------- Cycle ----------
const unsigned long HOLD_MS = 5000;
const unsigned long SCAN_TIMEOUT_MS = 0;   // 0 = scan indefinitely
const float ALIGN_LEAD_DEG = 3.0f;
const float ALIGN_ACCEPT_DEG = 5.0f;
const unsigned long ALIGN_TIMEOUT_MS = 8000;
const unsigned long ALIGN_SETTLE_MS = 150;
const int MAX_AUTO_RETRIES = 3;            // automatic restarts after a fault (reset after a good cycle)
const unsigned long FAULT_RETRY_MS = 5000;

// ---------- Laser / telemetry ----------
const bool LASER_ENABLED = true;           // laser is ON only during the HOLD state
const unsigned long TELEMETRY_MS = 500;    // 0 = off

// ---------- MPU6050 ----------
const uint8_t MPU_ADDR = 0x68;
const uint8_t REG_WHO_AM_I = 0x75;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_GYRO_XOUT_H = 0x43;
const int GYRO_AXIS = 2;                   // 0 = X, 1 = Y, 2 = Z (parallel to the spin axis)
const int GYRO_RANGE_SEL = 2;              // 0 = +/-250, 1 = +/-500, 2 = +/-1000, 3 = +/-2000 deg/s
const float GYRO_LSB[4] = {131.0f, 65.5f, 32.8f, 16.4f};
const unsigned long GYRO_POLL_MS = 10;
const int GYRO_BIAS_SAMPLES = 200;
const float GYRO_STILL_TOL_DPS = 4.0f;
const float GYRO_DEADBAND_DPS = 0.3f;
const unsigned long GYRO_REBIAS_SETTLE_MS = 1000;
const bool CROSSCHECK_FAULT = true;
const float CROSSCHECK_MIN_DEG = 8.0f;
const float CROSSCHECK_FRACTION = 0.15f;

// ---------- State ----------
enum State { ST_ROTATING, ST_ALIGN, ST_HOLD, ST_FAULT };
enum Fault { F_NONE, F_NO_MOTION, F_SENSOR_STUCK, F_ALIGN_FAIL, F_POS_MISMATCH, F_TIMEOUT };

State state = ST_FAULT;       // safe state until setup() finishes (laser OFF, motor OFF)
Fault fault = F_NONE;
unsigned long stateSinceMs = 0;
int faultRetries = 0;

bool laserOn = false;
bool motorPositive = true;
int motorPwm = 0;

volatile long encoderCount = 0;
int rotateSign = 1;
bool signLearned = false;

bool gyroOk = false;
int gyroFailRun = 0;
float gyroBiasDps = 0.0f;
float gyroRawDeg = 0.0f;
float gyroRateDps = 0.0f;
int gyroSign = 1;
bool gyroSignLearned = false;
unsigned long lastGyroUs = 0;

float holdSum = 0.0f, holdMin = 0.0f, holdMax = 0.0f;
int holdN = 0;

unsigned long lastSampleUs = 0;
unsigned long windowStartMs = 0;
float windowStartPose = NAN;
uint16_t count = 0;
uint16_t samples = 0;
uint16_t lastWinCount = 0;
uint16_t lastWinSamples = 0;

bool armed = false;
int clearRun = 0;
int confirmRun = 0;
unsigned long lowRunMs = 0;
float candidateStartPose = NAN;

unsigned long rotateStartMs = 0;
unsigned long motionArmedAtMs = 0;
bool motionRefValid = false;
unsigned long motionRefMs = 0;
long motionRefCounts = 0;
float motionRefGyroRaw = 0.0f;
int mismatchRun = 0;

long detections = 0;
float detStopPose = NAN;
float targetBearingDeg = NAN;
float angleErrorDeg = 0.0f;
float alignStartPose = NAN;
long alignStartCounts = 0;
float alignStartGyroRaw = 0.0f;
bool alignSettling = false;
unsigned long alignSettleStartMs = 0;
float alignRemainingDeg = 0.0f;

unsigned long holdStartMs = 0;
float holdStartPose = NAN;

char telemBuf[300];
uint16_t telemLen = 0, telemPos = 0;
unsigned long lastTelemMs = 0;

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
  if (!mpuWriteReg(REG_PWR_MGMT_1, 0x00)) return false;
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

// ---------- Pose (angle, increasing with DIR = HIGH) ----------
bool poseAvailable() {
  return COUNTS_PER_REV > 0 || gyroOk;
}

float poseDeg() {
  if (COUNTS_PER_REV > 0) return countsToDeg(readCounts());
  if (gyroOk) return gyroDegNow();
  return NAN;
}

// Detection is armed only once the sign of the active angle source is known.
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
// while turning to the target, in a fault and at boot.
void applyLaser() {
  bool want = LASER_ENABLED && (state == ST_HOLD);
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
  count = 0;
  samples = 0;
  windowStartMs = now;
  windowStartPose = poseDeg();
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

void enterFault(Fault f) {
  motorStop();
  digitalWrite(MOTOR_SLEEP_PIN, LOW);
  fault = f;
  setState(ST_FAULT); // laser goes OFF here
  Serial.print(F("!!! FAULT: "));
  Serial.println(faultShort(f));
  if (faultRetries < MAX_AUTO_RETRIES) {
    Serial.print(F("auto-restart in "));
    Serial.print(FAULT_RETRY_MS / 1000);
    Serial.println(F(" s"));
  } else {
    Serial.println(F("retries used up - staying safe (power-cycle to restart)"));
  }
}

void enterRotating() {
  unsigned long now = millis();
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);
  fault = F_NONE;
  armed = false;
  clearRun = 0;
  confirmRun = 0;
  mismatchRun = 0;
  lowRunMs = 0;
  rotateStartMs = now;
  motionRefValid = false;
  motionArmedAtMs = now + MOTION_GRACE_MS;
  setState(ST_ROTATING);          // laser OFF before the motor starts
  resetWindow(now);
  motorRun(SCAN_DIR > 0, SPIN_SPEED);
  Serial.println(F(">>> SCANNING (laser OFF) <<<"));
}

void enterHold() {
  motorStop();
  holdStartMs = millis();
  holdStartPose = poseDeg();
  holdSum = 0.0f;
  holdN = 0;
  setState(ST_HOLD);              // laser ON here, at the stopped target position
  Serial.print(F(">>> HOLD "));
  Serial.print(HOLD_MS / 1000.0f, 1);
  Serial.println(F(" s - laser ON, marking target <<<"));
}

// Bearing from the fixed centre. Scanning in direction SCAN_DIR, the sensor first reaches K
// when the target is EDGE_HALF_DEG ahead of its axis:
//   bearing = theta_first_seen + mount + SCAN_DIR * alpha
float estimateBearingDeg(float firstSeenPose) {
  float alpha = EDGE_HALF_DEG;
  if (SENSOR_RADIUS_MM > 0 && TARGET_DISTANCE_MM > 0) {
    float a = EDGE_HALF_DEG * DEG_TO_RAD;
    alpha = atan2f(TARGET_DISTANCE_MM * sinf(a),
                   SENSOR_RADIUS_MM + TARGET_DISTANCE_MM * cosf(a)) * RAD_TO_DEG;
  }
  return wrap360(firstSeenPose + SENSOR_MOUNT_DEG + (float)SCAN_DIR * alpha);
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
    Serial.println(F("ALIGN_ENABLED = false: not rotating, laser ON at the stopped position"));
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
  setState(ST_ALIGN);             // laser still OFF while turning
  motorRun(angleErrorDeg > 0, ALIGN_SPEED);
  Serial.println(F(">>> ROTATING TO TARGET (laser OFF) <<<"));
}

void acceptDetection() {
  motorStop();          // stop first, before any maths or printing
  gyroPoll(true);       // fresh gyro angle at the stop instant
  detections++;
  long stopCounts = readCounts();
  detStopPose = poseDeg();
  targetBearingDeg = isnan(candidateStartPose) ? NAN : estimateBearingDeg(candidateStartPose);

  Serial.print(F("DETECT #"));
  Serial.print(detections);
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
  lastWinCount = count;
  lastWinSamples = samples;
  bool hit = count >= DETECT_K;

  // sensor validation: solid LOW for too long means stuck / flooded
  if (samples > 0 && (unsigned long)count * 20UL >= (unsigned long)samples * 19UL) {
    lowRunMs += WINDOW_MS;
    if (lowRunMs >= STUCK_MS) { enterFault(F_SENSOR_STUCK); return; }
  } else {
    lowRunMs = 0;
  }

  // not armed: wait until the sensor is clear, so the same target is never detected twice
  if (!armed) {
    clearRun = hit ? 0 : clearRun + 1;
    if (clearRun >= REARM_CLEAR_WINDOWS && poseSignsReady()) {
      armed = true;
      Serial.println(F("armed"));
    }
    return;
  }

  if (!hit) {
    confirmRun = 0;
    return;
  }

  if (confirmRun == 0) candidateStartPose = windowStartPose; // where the sensor first reached K
  if (++confirmRun >= CONFIRM_WINDOWS) {
    acceptDetection();
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
    rotateSign = ((dEnc > 0) == (SCAN_DIR > 0)) ? 1 : -1;
    signLearned = true;
    Serial.print(F("encoder sign learned: "));
    Serial.println(rotateSign);
  }
  if (gyroMoved && !gyroSignLearned && fabsf(dGyroRaw) >= 5.0f) {
    gyroSign = ((dGyroRaw > 0) == (SCAN_DIR > 0)) ? 1 : -1;
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

// ---------- Telemetry (non-blocking: built in a buffer, fed to the UART as space frees up) ----------
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

// One line, e.g.
// Acc(g) X:0.04 Y:-0.19 Z:0.97 | Gyro(dps) X:-1.5 Y:0.4 Z:-1.4 | T:26.6C | Enc:1234cnt 45.2deg | GyroAng:44.8deg |
// Sig:7/200lo conf2/3 | Det:3 Brg:155.0deg LaserAx:0.0deg Err:+141.0deg | Mot:CW pwm200 | Laser:OFF | SCAN armed
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

  putP(F(" | Sig:")); putL(lastWinCount); putS("/"); putL(lastWinSamples);
  putP(F("lo conf")); putL(state == ST_ROTATING ? confirmRun : 0); putS("/"); putL(CONFIRM_WINDOWS);

  float pose = poseDeg();
  bool live = (state == ST_ALIGN || state == ST_HOLD) && !isnan(targetBearingDeg) && !isnan(pose);
  putP(F(" | Det:")); putL(detections);
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

// ---------- Arduino entry points ----------
void setup() {
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);        // laser explicitly OFF first
  laserOn = false;

  Serial.begin(9600);                  // output only: no serial input is used
  delay(500);

  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);

  Serial.println(F("=== Spin Detect One v0.1 (sensor=D6, laser=D10, MPU6050=A4/A5) ==="));
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
    Serial.println(F("NOTE: ALIGN_ENABLED = false - stop in place, laser ON there for the hold"));
  }
  delay(500);

  enterRotating();                     // automatic start; laser stays OFF
}

void loop() {
  unsigned long now = millis();

  gyroPoll(false);
  applyLaser();
  pumpTelemetry(now);

  switch (state) {
    case ST_ROTATING: {
      unsigned long nowUs = micros();
      if (nowUs - lastSampleUs >= SAMPLE_PERIOD_US) {
        lastSampleUs = nowUs;
        if (digitalRead(SENSOR_PIN) == LOW) count++;
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
      }
      break;
    }

    case ST_ALIGN: {
      float cur = poseDeg();
      if (isnan(cur)) { enterFault(F_ALIGN_FAIL); break; }
      float moved = cur - alignStartPose;
      float progress = (angleErrorDeg > 0) ? moved : -moved; // + = toward target
      float remaining = fabsf(angleErrorDeg) - progress;
      alignRemainingDeg = (angleErrorDeg > 0) ? remaining : -remaining;

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
        faultRetries = 0;              // a complete cycle: clear the retry budget
        enterRotating();               // laser turns OFF before the motor restarts
      }
      break;

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
