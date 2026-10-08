// ============================================================
// SPIN DETECT CLOCK (v0.2) - spin_detect with a fast "compare and centre" PID - Nano V3
// Build/upload with: pio run -e spin_detect_clock -t upload
// Then:              pio device monitor -e spin_detect_clock     (115200 baud)
//
// Same as spin_detect (scan, K = 5 per 100 ms, 4 s stop with the laser ON,
// laser OFF while moving, Acc/Gyro output). The rotation and the stop are new:
//
//   * D6 is the CENTRE sensor on the laser axis. D4 = anti-clockwise side,
//     D5 = clockwise side.
//   * The sensors are read in 10 ms slices; the decision is re-made EVERY
//     slice (100 times per second) from the last 50 ms of counts.
//   * Compare: which sensor has the greatest count?
//       - D6 is the greatest (within a small margin) -> STOP at once
//         (3 slices = 30 ms), then the laser is ON for 4 s.
//       - a side sensor is greater -> turn D6 TOWARD it (D4: anti-clockwise,
//         D5: clockwise). The PID output sets the speed from the size of the
//         difference between that sensor and D6:
//              e = weight * (side - D6) / (side + D6)       setpoint 0, + = clockwise
//              u = Kp*e + Ki*integral(e) + Kd*de/dt
//   * No stop and no laser if D6 never becomes the greatest: after
//     TRACK_TIMEOUT_MS it gives up and goes back to scanning.
//
// Output (every 100 ms; counts are the last 100 ms):
//   S1(D4): 5  S2(D5): 40  S3(D6): 60  DETECTED | Acc(g) X:.. | Gyro(dps) X:.. | TURN CW pwm130 e:+0.40
//
// MPU6050 (display only): VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68).
// Maths, PID tuning, bench procedure: spin_detect_clock.md
// ============================================================
#include <Arduino.h>
#include <Wire.h>

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
const int SPIN_SPEED = 200;                       // scanning PWM (spin_until_ir value)
const bool SCAN_CLOCKWISE = true;
const unsigned long REVERSE_PAUSE_MS = 30;        // pause before the motor changes direction

// ---------- Fast counting: 10 ms slices, rolling windows ----------
const unsigned long SLICE_MS = 10;
const int RING = 10;                              // 10 slices = 100 ms
const int FAST_SLICES = 5;                        // 5 slices = 50 ms: used for the compare / stop
const int DETECT_THRESHOLD = 5;                   // LOW reads per 100 ms = signal (spin_until_ir value)
const int TRACK_THRESHOLD = 3;                    // LOW reads per 50 ms (same duty)
const int CLEAR_SLICES_TO_REARM = 20;             // 200 ms without signal before the next detection

// ---------- Compare / stop ----------
const int CENTRE_SLICES = 3;                      // D6 greatest for 3 slices (30 ms) -> stop
const int CENTRE_PERCENT = 100;                   // D6 wins when D6 + MIN_MARGIN >= CENTRE_PERCENT % of the strongest OTHER sensor.
                                                  // 100 = D6 is at least as strong (always stops); 130-150 = D6 clearly
                                                  // stronger -> centres tighter, but needs beams that overlap less.
const int MIN_MARGIN = 2;                         // counts of tolerance (noise)
const unsigned long TRACK_TIMEOUT_MS = 4000;      // D6 never became the greatest -> give up (no stop, no laser)
const int LOST_SLICES = 50;                       // no signal for 0.5 s -> back to scanning

// ---------- PID (speed of the turn toward the greatest sensor) ----------
const float KP = 140.0f;                          // PWM per unit of error
const float KI = 40.0f;                           // PWM per (unit of error x second)
const float KD = 6.0f;                            // PWM per (unit of error / second)
const float I_MAX = 1.0f;                         // integral limit (anti-windup)
const int MIN_TRACK_PWM = 110;                    // smallest PWM that still turns the assembly
const int MAX_TRACK_PWM = 200;

// ---------- Stop ----------
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
int c50[NUM_SENSORS];                             // last 50 ms (compare / stop)

bool armed = false;
int clearSlices = 0;
int centreRun = 0;
int lostSlices = 0;
bool centreSeen = false;

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
  analogWrite(MOTOR_PWM_PIN, speed);
  motorDir = dir;
  motorPwm = speed;
}

void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  motorPwm = 0;
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
      if (!armed) Serial.print(F(" arming"));
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
  armed = false;                                  // the signal must clear before the next detection
  clearSlices = 0;
  Serial.println(F(">>> STOP finished - laser OFF, scanning <<<"));
  startScan();
}

void startTurn() {
  turnStartMs = millis();
  lastPidMs = turnStartMs;
  lostSlices = 0;
  centreRun = 0;
  centreSeen = false;
  integral = 0.0f;
  dFilt = 0.0f;
  ePrev = 0.0f;
  lastErr = 0.0f;
  state = ST_TURN;
  int b = strongest(c100);
  Serial.print(F(">>> message on S"));
  Serial.print(b + 1);
  Serial.println(F(": comparing sensors, turning D6 toward the greatest <<<"));
}

int clampPwm(float u) {
  int p = (int)(u < 0 ? -u : u);
  if (p < MIN_TRACK_PWM) p = MIN_TRACK_PWM;       // keep turning until D6 is the greatest
  if (p > MAX_TRACK_PWM) p = MAX_TRACK_PWM;
  return p;
}

// ---------- Tracking: runs every 10 ms slice ----------
void turnStep(unsigned long now) {
  if (now - turnStartMs >= TRACK_TIMEOUT_MS) {
    if (!centreSeen) {
      Serial.println(F(">>> WARNING: D6 never received the message - check D6 wiring / aim <<<"));
    }
    Serial.println(F(">>> turn timeout - D6 not the greatest, back to scanning (no stop, no laser) <<<"));
    armed = false;
    clearSlices = 0;
    startScan();
    return;
  }

  int cb = c50[strongest(c50)];
  int c6 = c50[CENTRE_SENSOR];
  if (c6 >= TRACK_THRESHOLD) centreSeen = true;

  if (cb < TRACK_THRESHOLD) {                     // nothing above the noise: keep going the same way
    if (++lostSlices >= LOST_SLICES) {
      Serial.println(F(">>> message lost - scanning <<<"));
      startScan();
    }
    return;
  }
  lostSlices = 0;

  // ---- compare: is D6 the greatest? (against the strongest OTHER sensor) ----
  int side = -1;                                  // strongest sensor other than D6
  int cOther = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (i != CENTRE_SENSOR && (side < 0 || c50[i] > cOther)) {
      side = i;
      cOther = c50[i];
    }
  }
  if (c6 >= TRACK_THRESHOLD && (long)(c6 + MIN_MARGIN) * 100L >= (long)cOther * CENTRE_PERCENT) {
    if (motorPwm > MIN_TRACK_PWM) driveMotor(motorDir, MIN_TRACK_PWM);   // slow down while confirming
    if (++centreRun >= CENTRE_SLICES) startStop(F("D6 is the greatest"));
    return;
  }
  centreRun = 0;

  // ---- move D6 toward the greatest other sensor; PID sets the speed ----
  float mag = (float)(cOther - c6) / (float)(cOther + c6 + 1);   // 0 .. 1: how much stronger that sensor is
  if (mag < 0.05f) mag = 0.05f;                   // D6 leads but not clearly yet: creep toward the stronger side
  float e = (float)SENSOR_WEIGHT[side] * mag;
  float dt = (float)(now - lastPidMs) / 1000.0f;
  lastPidMs = now;
  if (dt < 0.001f) dt = 0.001f;

  if ((e > 0.0f) != (ePrev > 0.0f)) integral = 0.0f;   // other side became the greatest: drop the history
  integral += e * dt;
  if (integral > I_MAX) integral = I_MAX;
  if (integral < -I_MAX) integral = -I_MAX;

  float d = (e - ePrev) / dt;
  dFilt = 0.8f * dFilt + 0.2f * d;
  ePrev = e;
  lastErr = e;

  float u = KP * e + KI * integral + KD * dFilt;
  int dir = SENSOR_WEIGHT[side] > 0 ? 1 : -1;     // always toward the stronger side sensor
  int pwm = clampPwm(u);
  if (dir != motorDir || pwm != motorPwm) driveMotor(dir, pwm);
}

void setup() {
  pinMode(LASER_PIN, OUTPUT);
  laserSet(false);                                // laser explicitly OFF first

  Serial.begin(115200);
  delay(500);

  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);            // wake the driver - required or nothing spins
  analogWrite(MOTOR_PWM_PIN, 0);

  for (int i = 0; i < NUM_SENSORS; i++) pinMode(SENSOR_PINS[i], INPUT_PULLUP);

  Serial.println(F("=== Spin Detect Clock v0.2 (D4 left, D5 right, D6 centre + laser D10, MPU6050=A4/A5) ==="));
  imuOk = imuInit();
  Serial.println(imuOk ? F("MPU6050 found at 0x68") : F("MPU6050 not found at 0x68 (IMU:OFF) - continuing"));
  Serial.println(F("Auto-starting: scan, compare sensors, turn D6 to the greatest, stop 4 s with laser ON"));
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
        if (det) {
          clearSlices = 0;
        } else if (++clearSlices >= CLEAR_SLICES_TO_REARM) {
          armed = true;
        }
      } else if (det) {
        startTurn();                              // the compare / stop logic takes over next slice
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
