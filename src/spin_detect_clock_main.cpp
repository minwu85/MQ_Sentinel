// ============================================================
// SPIN DETECT CLOCK (v0.1) - spin_detect with a PID rotation - Nano V3
// Build/upload with: pio run -e spin_detect_clock -t upload
// Then:              pio device monitor -e spin_detect_clock     (115200 baud)
//
// Same as spin_detect (scan, 100 ms count windows, K = 5, 4 s stop with
// the laser ON, laser OFF while moving, Acc/Gyro output) - only the
// rotation after a detection is new:
//
//   * D6 is the CENTRE sensor and sits on the laser axis. The goal is to
//     keep D6 pointing at the message.
//   * D4 = side sensor on the anti-clockwise side, D5 = clockwise side.
//   * After a detection a PID loop rotates the assembly (like a clock
//     hand) until D6 is centred on the message, then it stops:
//
//        position  pos = (D5 - D4) / (D4 + D5 + D6)      -1 .. +1  (+ = message clockwise of D6)
//        setpoint  0   (message centred on D6)
//        error     e   = setpoint - (-pos) = pos
//        output    u   = Kp*e + Ki*integral(e) + Kd*de/dt      u > 0 -> clockwise, u < 0 -> anti-clockwise
//        PWM       |u| limited to MIN_TRACK_PWM .. MAX_TRACK_PWM (strong error = fast, weak = slow)
//
//   * Signal strength is also used: D6 must be near its peak count
//     (PEAK_FRACTION) before it stops; if it has passed the peak it
//     backs up toward it.
//   * D6 must also have stopped getting stronger (peak reached) before it stops.
//   * The motor keeps turning until D6 is centred; it only gives up after
//     TRACK_TIMEOUT_MS (stop + laser) or if the message disappears.
//
// Output (every window):
//   S1(D4): 5  S2(D5): 40  S3(D6): 60  DETECTED | Acc(g) X:.. | Gyro(dps) X:.. | TURN CW pwm130 e:+0.40
//
// MPU6050 (display only): VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68).
// Maths, PID tuning, bench procedure: spin_detect_clock.md
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
// weight: -1 = on the anti-clockwise side of the centre, +1 = clockwise side, 0 = centre (laser axis)
const int SENSOR_WEIGHT[NUM_SENSORS] = {-1, +1, 0};
const int CENTRE_SENSOR = 2;                      // D6

// ---------- Motor ----------
const int SPIN_SPEED = 200;                       // scanning PWM (spin_until_ir value)
const bool SCAN_CLOCKWISE = true;
const unsigned long REVERSE_PAUSE_MS = 40;        // pause before the motor changes direction

// ---------- Detection while scanning (spin_until_ir approach) ----------
const unsigned long WINDOW_MS = 100;
const int DETECT_THRESHOLD = 5;                   // LOW reads per 100 ms window = signal
const int CONFIRM_WINDOWS = 1;
const int CLEAR_WINDOWS_TO_REARM = 2;
const int ALIGN_MARGIN = 8;                       // centre within this of the strongest

// ---------- PID rotation (tracking) ----------
const unsigned long TRACK_WINDOW_MS = 50;         // faster loop while turning (20 Hz)
const int TRACK_THRESHOLD = 3;                    // LOW reads per 50 ms window (same duty as K = 5 per 100 ms)
const int TRACK_ALIGN_MARGIN = 4;
const float KP = 140.0f;                          // PWM per unit of error
const float KI = 40.0f;                           // PWM per (unit of error x second)
const float KD = 6.0f;                            // PWM per (unit of error / second)
const float I_MAX = 1.0f;                         // integral limit (anti-windup)
const int MIN_TRACK_PWM = 110;                    // smallest PWM that still turns the assembly
const int MAX_TRACK_PWM = 200;
const float ALIGN_DEADBAND = 0.20f;               // |pos| below this = centred
const float PEAK_FRACTION = 0.8f;                 // D6 must be >= 80 % of its peak count to stop
const float STRENGTH_GAIN = 0.5f;                 // back-up error when D6 is past its peak
const int RISE_MARGIN = 2;                        // D6 count rising by more than this = still climbing to its peak
const int ALIGN_CONFIRM_WINDOWS = 2;              // centred and at the peak for this many windows (2 x 50 ms)
const unsigned long TRACK_TIMEOUT_MS = 6000;      // give up turning -> stop here (laser ON)
const int LOST_WINDOWS = 10;                      // message gone for 10 x 50 ms -> back to scanning

// ---------- Stop ----------
const unsigned long STOP_MS = 4000;               // stop time, laser ON for all of it

// ---------- MPU6050 (display only) ----------
const uint8_t MPU_ADDR = 0x68;
const float GYRO_LSB = 32.8f;                     // +/-1000 deg/s range
const unsigned long IMU_RETRY_MS = 2000;

// ---------- State ----------
enum State { ST_SCAN, ST_TURN, ST_STOP };
State state = ST_SCAN;

int counts[NUM_SENSORS];
int best = 0;
bool detected = false;
bool aligned = false;
float pos = 0.0f;

bool armed = false;
int clearRun = 0;
int detectRun = 0;
int alignedRun = 0;
int lostRun = 0;

int motorDir = 1;                                 // +1 clockwise, -1 anti-clockwise
int motorPwm = 0;
unsigned long turnStartMs = 0;
unsigned long stopEndMs = 0;

float integral = 0.0f;
float ePrev = 0.0f;
float dFilt = 0.0f;
float lastErr = 0.0f;
int peakCentre = 0;
int prevCentre = 0;

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

// strongest sensor, detection, position of the message relative to the centre, aligned flag
void analyse(int thr, int margin) {
  best = 0;
  for (int i = 1; i < NUM_SENSORS; i++) {
    if (counts[i] > counts[best]) best = i;
  }
  detected = counts[best] >= thr;

  float sum = 0.0f, weighted = 0.0f;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (counts[i] >= thr) {                       // ignore sub-threshold noise
      sum += counts[i];
      weighted += (float)SENSOR_WEIGHT[i] * counts[i];
    }
  }
  pos = (sum > 0.0f) ? weighted / sum : 0.0f;     // -1 .. +1, + = message clockwise of the centre

  int c = counts[CENTRE_SENSOR];
  aligned = detected && c >= thr && c + margin >= counts[best] && fabsf(pos) <= ALIGN_DEADBAND;
}

// ---------- Output ----------
void printLine(unsigned long now) {
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
  armed = false;                                  // the signal must clear before the next detection
  clearRun = 0;
  Serial.println(F(">>> STOP finished - laser OFF, scanning <<<"));
  startScan();
}

int clampPwm(float u) {
  int p = (int)fabsf(u);
  if (p < MIN_TRACK_PWM) p = MIN_TRACK_PWM;       // always keep turning until D6 is centred
  if (p > MAX_TRACK_PWM) p = MAX_TRACK_PWM;
  return p;
}

void startTurn() {
  turnStartMs = millis();
  lostRun = 0;
  alignedRun = 0;
  integral = 0.0f;
  dFilt = 0.0f;
  ePrev = pos;
  lastErr = pos;
  peakCentre = counts[CENTRE_SENSOR];
  prevCentre = 0;                                 // first tracking window counts as 'still rising'
  state = ST_TURN;
  int dir = (pos > 0.0f) ? 1 : (pos < 0.0f ? -1 : (SCAN_CLOCKWISE ? 1 : -1));
  Serial.print(F(">>> message on S"));
  Serial.print(best + 1);
  Serial.print(F(": PID turn "));
  Serial.print(dir > 0 ? F("clockwise") : F("anti-clockwise"));
  Serial.println(F(" until D6 is centred <<<"));
  driveMotor(dir, clampPwm(KP * fabsf(pos)));
}

// One PID update per tracking window.
void pidStep(float dt) {
  int c = counts[CENTRE_SENSOR];
  bool nearPeak = c >= PEAK_FRACTION * peakCentre;

  float e = pos;                                  // setpoint 0 minus (-pos)
  if (c >= TRACK_THRESHOLD && fabsf(pos) <= ALIGN_DEADBAND && !nearPeak) {
    // centred between the sides but past the peak of D6: back up toward the peak
    e = -(float)motorDir * STRENGTH_GAIN * (1.0f - (float)c / (float)peakCentre);
  }

  if ((e > 0.0f) != (ePrev > 0.0f)) integral = 0.0f;   // error changed sign: drop the history
  integral += e * dt;
  if (integral > I_MAX) integral = I_MAX;
  if (integral < -I_MAX) integral = -I_MAX;

  float d = (e - ePrev) / dt;
  dFilt = 0.5f * dFilt + 0.5f * d;
  ePrev = e;
  lastErr = e;

  float u = KP * e + KI * integral + KD * dFilt;
  int dir = (u > 0.0f) ? 1 : (u < 0.0f ? -1 : motorDir);
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

  Serial.println(F("=== Spin Detect Clock v0.1 (D4 left, D5 right, D6 centre + laser D10, MPU6050=A4/A5) ==="));
  imuOk = imuInit();
  Serial.println(imuOk ? F("MPU6050 found at 0x68") : F("MPU6050 not found at 0x68 (IMU:OFF) - continuing"));
  Serial.println(F("Auto-starting: scan, detect, PID-rotate until D6 is centred, stop 4 s with laser ON"));
  delay(1000);

  startScan();
}

void loop() {
  unsigned long dur = WINDOW_MS;
  if (state == ST_TURN) dur = TRACK_WINDOW_MS;
  if (state == ST_STOP) {
    long left = (long)(stopEndMs - millis());
    if (left <= 0) {
      endStop();
      return;
    }
    if ((unsigned long)left < dur) dur = left;    // end the stop exactly on time
  }

  sampleWindow(dur);
  if (state == ST_TURN) analyse(TRACK_THRESHOLD, TRACK_ALIGN_MARGIN);
  else analyse(DETECT_THRESHOLD, ALIGN_MARGIN);
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
          startTurn();                                // always refine with the PID, even if D6 already sees it
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
          Serial.println(F(">>> message lost - scanning <<<"));
          startScan();
        }                                          // otherwise keep turning the same way
      } else {
        lostRun = 0;
        int c = counts[CENTRE_SENSOR];
        if (c > peakCentre) peakCentre = c;
        bool rising = c > prevCentre + RISE_MARGIN;   // D6 still getting stronger: not at its peak yet
        prevCentre = c;
        if (aligned && c >= PEAK_FRACTION * peakCentre && !rising) {
          if (++alignedRun >= ALIGN_CONFIRM_WINDOWS) startStop(F("D6 centred on the message"));
        } else if (aligned && rising) {
          alignedRun = 0;                              // centred between the sides: keep going up to D6's peak
          lastErr = 0.0f;
          ePrev = 0.0f;
          integral = 0.0f;
          driveMotor(motorDir, MIN_TRACK_PWM);
        } else {
          alignedRun = 0;
          pidStep((float)dur / 1000.0f);
        }
      }
      break;

    case ST_STOP:
      break;
  }

  printLine(now);
}
