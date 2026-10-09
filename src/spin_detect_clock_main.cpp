// ============================================================
// SPIN DETECT CLOCK (v0.5) - clock rotation (centroid PID), guaranteed stop, SLOW speed, laser ON = motor OFF - Nano V3
// Build/upload with: pio run -e spin_detect_clock -t upload
// Then:              pio device monitor -e spin_detect_clock     (115200 baud)
//
// Same as spin_detect (scan, K = 5 per 100 ms, 4 s stop with the laser ON,
// laser OFF while moving, Acc/Gyro output). Rotation = the v0.1 "clock" PID:
//
//   * D6 is the CENTRE sensor on the laser axis. D4 = anti-clockwise side,
//     D5 = clockwise side.
//   * position   pos = (D5 - D4) / (D4 + D5 + D6)       -1 .. +1, + = message clockwise of D6
//     setpoint 0, error e = pos, u = Kp*e + Ki*integral(e) + Kd*de/dt
//     (PWM = |u| limited to MIN_TRACK_PWM .. MAX_TRACK_PWM, direction = sign of pos)
//
// Why v0.3 never stopped (found by analysis): the detection had to be
// "armed", and arming needed 200 ms with NO signal. With three sensors
// 120 deg apart, spinning at 200 PWM (about 180 deg/s), the gap between the
// beams lasts only about 170-190 ms, so the system never armed, never
// detected, never stopped and never switched the laser on.
//
// v0.4 fixes:
//   * armed from power-up; after a stop it re-arms after 100 ms without
//     signal OR after REARM_DELAY_MS (700 ms of scanning) - whichever first
//   * every detection ENDS IN A STOP: the turn has a hard time limit
//     (FORCE_STOP_MS) that does not depend on the sensors; a message that
//     flickers or sits in a gap between beams no longer aborts the turn
//   * stop as soon as the message is centred: |pos| inside a dead band that
//     widens with time (0.15 -> 0.60 over 1.2 s), or pos crossed zero, for
//     3 slices (30 ms), decided every 10 ms
//   * direction follows the sign of pos with hysteresis (no chatter);
//     the PID only sets the speed
//
// v0.5: SLOW speed (scan 130, turn 100..140 PWM, was 200 / 110..200) and the LASER RULE
//   laser ON whenever the motor is OFF (power-up and every stop), OFF whenever it runs.
//   Slower turning needs longer limits: FORCE_STOP_MS 3000, RELAX_MS 2400, REARM_DELAY_MS 1500.
//
// Output (every 100 ms; counts are the last 100 ms):
//   S1(D4): 5  S2(D5): 40  S3(D6): 60  DETECTED | Acc(g) X:.. | Gyro(dps) X:.. | TURN CW pwm130 e:+0.40
//
// MPU6050 (display only): VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68).
// Maths, flowcharts, version history, code explanation: spin_detect_clock.md
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ---------- Pins ----------
const int NUM_SENSORS = 3;
const int SENSOR_PINS[NUM_SENSORS] = {4, 5, 6};   // S1(D4), S2(D5), S3(D6) (TSOP4138, active-LOW)

const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;   // HIGH = clockwise (as motor_onoff)
const int MOTOR_PWM_PIN   = 9;
const int LASER_PIN       = 10;  // HIGH = laser ON

// ---------- Sensor layout (CONFIGURE to your wiring) ----------
// weight: -1 = anti-clockwise side of the centre, +1 = clockwise side, 0 = centre (laser axis)
const int SENSOR_WEIGHT[NUM_SENSORS] = {-1, +1, 0};
const int CENTRE_SENSOR = 2;                      // D6

// ---------- Motor ----------
const int SPIN_SPEED = 130;                       // scanning PWM - SLOW (was 200); raise if the motor stalls
const bool SCAN_CLOCKWISE = true;
const unsigned long REVERSE_PAUSE_MS = 30;        // pause before the motor changes direction

// ---------- Fast counting: 10 ms slices, rolling windows ----------
const unsigned long SLICE_MS = 10;
const int RING = 10;                              // 10 slices = 100 ms
const int FAST_SLICES = 5;                        // 5 slices = 50 ms: used while turning / stopping
const int DETECT_THRESHOLD = 5;                   // LOW reads per 100 ms = signal (spin_until_ir value)
const int TRACK_THRESHOLD = 3;                    // LOW reads per 50 ms (same duty)

// ---------- Re-arming after a stop ----------
const int CLEAR_SLICES_TO_REARM = 10;             // 100 ms without signal ...
const unsigned long REARM_DELAY_MS = 1500;        // ... or this long after the stop (about 130 deg of slow scanning)

// ---------- PID rotation (v0.1 clock rotation) ----------
const float KP = 140.0f;                          // PWM per unit of error
const float KI = 40.0f;                           // PWM per (unit of error x second)
const float KD = 6.0f;                            // PWM per (unit of error / second)
const float I_MAX = 1.0f;                         // integral limit (anti-windup)
const int MIN_TRACK_PWM = 100;                    // smallest PWM that still turns the assembly (was 110)
const int MAX_TRACK_PWM = 140;                    // SLOW (was 200)
const float DIR_HYST = 0.10f;                     // |pos| below this keeps the current direction

// ---------- Stop ----------
const float DEAD_START = 0.15f;                   // |pos| <= this = centred (at the start of the turn) ...
const float DEAD_END = 0.60f;                     // ... widening to this
const unsigned long RELAX_MS = 2400;              // ... over this time (was 1200: slower turn needs longer)
const float SIGN_MIN = 0.05f;                     // |pos| above this counts as a side (for zero-crossing)
const int STOP_SLICES = 3;                        // condition true for 3 slices (30 ms) -> stop
const unsigned long FORCE_STOP_MS = 3000;         // HARD LIMIT: every turn ends in a stop after this long (was 1500)
const unsigned long STOP_MS = 4000;               // stop time, laser ON for all of it

// ---------- Output / MPU6050 (display only) ----------
const unsigned long PRINT_MS = 100;
const uint8_t MPU_ADDR = 0x68;
const float GYRO_LSB = 32.8f;                     // +/-1000 deg/s range
const unsigned long IMU_RETRY_MS = 2000;

// ---------- State ----------
enum State { ST_SCAN, ST_TURN, ST_STOP };
State state = ST_SCAN;

uint8_t ring[RING][NUM_SENSORS];
int ringHead = 0;
int c100[NUM_SENSORS];                            // last 100 ms (printed, scan detection)
int c50[NUM_SENSORS];                             // last 50 ms (position / stop)

bool armed = true;                                // armed from power-up
int clearSlices = 0;
unsigned long rearmAtMs = 0;
int stopRun = 0;
bool centreSeen = false;
bool crossed = false;
int lastSign = 0;

int motorDir = 1;                                 // +1 clockwise, -1 anti-clockwise
int motorPwm = 0;
unsigned long turnStartMs = 0;
unsigned long stopEndMs = 0;
unsigned long lastPidMs = 0;
unsigned long lastPrintMs = 0;

float integral = 0.0f;
float ePrev = 0.0f;
float dFilt = 0.0f;
float lastErr = 0.0f;

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

// ---------- Counting: one 10 ms slice, then rolling sums ----------
void sampleSlice(unsigned long ms) {
  uint8_t *slot = ring[ringHead];
  for (int i = 0; i < NUM_SENSORS; i++) slot[i] = 0;
  unsigned long start = millis();
  while (millis() - start < ms) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (digitalRead(SENSOR_PINS[i]) == LOW) slot[i]++;
    }
    delayMicroseconds(100);
  }
  ringHead = (ringHead + 1) % RING;

  for (int i = 0; i < NUM_SENSORS; i++) {
    int s100 = 0, s50 = 0;
    for (int k = 1; k <= RING; k++) {
      int v = ring[(ringHead - k + RING) % RING][i];
      s100 += v;
      if (k <= FAST_SLICES) s50 += v;
    }
    c100[i] = s100;
    c50[i] = s50;
  }
}

int strongest(const int *c) {
  int b = 0;
  for (int i = 1; i < NUM_SENSORS; i++) {
    if (c[i] > c[b]) b = i;
  }
  return b;
}

// Position of the message relative to D6: weighted mean of the sensor weights,
// -1 .. +1, + = clockwise of D6. Counts below the threshold (noise) are ignored.
float positionOf(const int *c, int thr) {
  float sum = 0.0f, weighted = 0.0f;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (c[i] >= thr) {
      sum += c[i];
      weighted += (float)SENSOR_WEIGHT[i] * c[i];
    }
  }
  return (sum > 0.0f) ? weighted / sum : 0.0f;
}

// ---------- Output ----------
void printLine(unsigned long now) {
  int b = strongest(c100);
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(F("S"));
    Serial.print(i + 1);
    Serial.print(F("(D"));
    Serial.print(SENSOR_PINS[i]);
    Serial.print(F("): "));
    Serial.print(c100[i]);
    Serial.print(F("  "));
  }
  Serial.print(c100[b] >= DETECT_THRESHOLD ? F("DETECTED") : F("--"));

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
      if (!armed) Serial.print(F(" rearming"));
      break;
    case ST_TURN:
      Serial.print(F("TURN "));
      Serial.print(motorDir > 0 ? F("CW") : F("CCW"));
      Serial.print(F(" pwm"));
      Serial.print(motorPwm);
      Serial.print(F(" e:"));
      if (lastErr >= 0) Serial.print('+');
      Serial.print(lastErr, 2);
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
  armed = false;                                  // re-arm: 100 ms without signal or REARM_DELAY_MS
  clearSlices = 0;
  rearmAtMs = millis() + REARM_DELAY_MS;
  Serial.println(F(">>> STOP finished - laser OFF, scanning <<<"));
  startScan();
}

int clampPwm(float u) {
  int p = (int)(u < 0 ? -u : u);
  if (p < MIN_TRACK_PWM) p = MIN_TRACK_PWM;       // keep turning until the message is centred
  if (p > MAX_TRACK_PWM) p = MAX_TRACK_PWM;
  return p;
}

void startTurn() {
  turnStartMs = millis();
  lastPidMs = turnStartMs;
  stopRun = 0;
  centreSeen = false;
  crossed = false;
  lastSign = 0;
  integral = 0.0f;
  dFilt = 0.0f;
  state = ST_TURN;

  float pos = positionOf(c100, DETECT_THRESHOLD);
  ePrev = pos;
  lastErr = pos;
  int dir = (pos > 0.0f) ? 1 : (pos < 0.0f ? -1 : (SCAN_CLOCKWISE ? 1 : -1));
  Serial.print(F(">>> message on S"));
  Serial.print(strongest(c100) + 1);
  Serial.print(F(": PID turn "));
  Serial.print(dir > 0 ? F("clockwise") : F("anti-clockwise"));
  Serial.println(F(" until D6 is centred <<<"));
  driveMotor(dir, clampPwm(KP * (pos < 0 ? -pos : pos)));
}

// ---------- Turning + stopping: runs every 10 ms slice; ALWAYS ends in a stop ----------
void turnStep(unsigned long now) {
  unsigned long el = now - turnStartMs;

  if (c50[CENTRE_SENSOR] >= TRACK_THRESHOLD) centreSeen = true;

  // hard limit: independent of the sensors, so the motor always stops (and the laser always comes on)
  if (el >= FORCE_STOP_MS) {
    if (!centreSeen) {
      Serial.println(F(">>> WARNING: D6 never received the message - check D6 wiring / aim <<<"));
    }
    startStop(F("time limit"));
    return;
  }

  if (c50[strongest(c50)] < TRACK_THRESHOLD) {    // message not on the sensors right now (burst gap, gap between beams)
    stopRun = 0;                                  // keep turning the same way; the hard limit still applies
    return;
  }

  float pos = positionOf(c50, TRACK_THRESHOLD);

  // ---- stop: centred (dead band that widens with time) or the message passed the centre ----
  float dead = DEAD_END;
  if (el < RELAX_MS) dead = DEAD_START + (DEAD_END - DEAD_START) * (float)el / (float)RELAX_MS;

  int sgn = (fabsf(pos) < SIGN_MIN) ? 0 : (pos > 0.0f ? 1 : -1);
  if (sgn != 0) {
    if (lastSign != 0 && sgn != lastSign) crossed = true;   // pos changed sign: passed the centre
    lastSign = sgn;
  }

  bool centred = (fabsf(pos) <= dead) || crossed;
  stopRun = centred ? stopRun + 1 : 0;

  if (stopRun >= STOP_SLICES) {
    startStop(F("D6 centred on the message"));
    return;
  }

  // ---- PID: turn D6 toward the message ----
  float dt = (float)(now - lastPidMs) / 1000.0f;
  lastPidMs = now;
  if (dt < 0.001f) dt = 0.001f;

  float e = pos;                                  // setpoint 0 - (-pos)
  if ((e > 0.0f) != (ePrev > 0.0f)) integral = 0.0f;   // error changed sign: drop the history
  integral += e * dt;
  if (integral > I_MAX) integral = I_MAX;
  if (integral < -I_MAX) integral = -I_MAX;

  float d = (e - ePrev) / dt;
  dFilt = 0.8f * dFilt + 0.2f * d;
  ePrev = e;
  lastErr = e;

  float u = KP * e + KI * integral + KD * dFilt;
  int dir = motorDir;                             // direction from the sign of pos, with hysteresis
  if (fabsf(pos) >= DIR_HYST) dir = (pos > 0.0f) ? 1 : -1;
  int pwm = clampPwm(u);                          // the PID sets the speed only
  if (centred && pwm > MIN_TRACK_PWM) pwm = MIN_TRACK_PWM;   // about to stop: slow down while confirming
  if (dir != motorDir || pwm != motorPwm) driveMotor(dir, pwm);
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

  Serial.println(F("=== Spin Detect Clock v0.5 (D4 left, D5 right, D6 centre + laser D10, MPU6050=A4/A5) ==="));
  imuOk = imuInit();
  Serial.println(imuOk ? F("MPU6050 found at 0x68") : F("MPU6050 not found at 0x68 (IMU:OFF) - continuing"));

  Serial.println(F("Laser is ON now (motor off); it goes OFF when the motor starts"));

  Serial.println(F("Auto-starting: scan, PID-rotate until D6 is centred, always stop, 4 s with laser ON"));
  delay(1000);

  startScan();
}

void loop() {
  unsigned long dur = SLICE_MS;
  if (state == ST_STOP) {
    long left = (long)(stopEndMs - millis());
    if (left <= 0) {
      endStop();
      return;
    }
    if ((unsigned long)left < dur) dur = left;    // end the stop exactly on time
  }

  sampleSlice(dur);
  unsigned long now = millis();

  switch (state) {
    case ST_SCAN: {
      bool det = c100[strongest(c100)] >= DETECT_THRESHOLD;
      if (!armed) {
        if (det) clearSlices = 0;
        else if (++clearSlices >= CLEAR_SLICES_TO_REARM) armed = true;
        if ((long)(now - rearmAtMs) >= 0) armed = true;   // never wait for a clear gap longer than REARM_DELAY_MS
      } else if (det) {
        startTurn();                              // the PID / stop logic takes over next slice
      }
      break;
    }

    case ST_TURN:
      turnStep(now);
      break;

    case ST_STOP:
      break;
  }

  if (now - lastPrintMs >= PRINT_MS) {
    lastPrintMs = now;
    printLine(now);
  }
}
