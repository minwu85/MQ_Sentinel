// ============================================================
// SPIN DETECT ONE (v1.0) - spin_detect with ONE sensor - Nano V3
// Build/upload with: pio run -e spin_detect_one -t upload
// Then:              pio device monitor -e spin_detect_one     (115200 baud)
//
// Everything is the same as spin_detect, but with a single sensor (D6)
// on the laser axis, so there is nothing to turn toward:
//   * the motor spins clockwise with the laser OFF
//   * the sensor counts LOW reads in 100 ms windows (K = 5, as spin_until_ir)
//   * on a detection the motor stops, then the laser is ON for the whole
//     4 s stop; then the laser goes OFF and scanning resumes
//   * the signal must clear (2 windows) before the next detection
//
// Output (every window):
//   S1(D6): 72  DETECTED | Acc(g) X:-0.68 Y:-0.60 Z:0.51 | Gyro(dps) X:-5.7 Y:2.7 Z:-3.8 | SCAN CW
//
// MPU6050 (display only): VCC 5V, GND common, SDA A4, SCL A5, AD0 GND (0x68).
// Maths, wiring, tuning: spin_detect_one.md
// ============================================================
#include <Arduino.h>
#include <Wire.h>

// ---------- Pins ----------
const int SENSOR_PIN = 6;        // S1(D6): TSOP4138, active-LOW, on the laser axis

const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;   // HIGH = clockwise (as motor_onoff)
const int MOTOR_PWM_PIN   = 9;
const int LASER_PIN       = 10;  // HIGH = laser ON

// ---------- Motor ----------
const int SPIN_SPEED = 200;                       // scanning PWM (spin_until_ir value)
const bool SCAN_CLOCKWISE = true;

// ---------- Detection (spin_until_ir approach) ----------
const unsigned long WINDOW_MS = 100;              // one counting window
const int DETECT_THRESHOLD = 5;                   // LOW reads in a window = signal (spin_until_ir value)
const int CONFIRM_WINDOWS = 1;                    // consecutive windows needed (2 = more noise immunity)
const int CLEAR_WINDOWS_TO_REARM = 2;             // quiet windows before the next detection

// ---------- Stop ----------
const unsigned long STOP_MS = 4000;               // stop time, laser ON for all of it

// ---------- MPU6050 (display only) ----------
const uint8_t MPU_ADDR = 0x68;
const float GYRO_LSB = 32.8f;                     // +/-1000 deg/s range
const unsigned long IMU_RETRY_MS = 2000;

// ---------- State ----------
enum State { ST_SCAN, ST_STOP };
State state = ST_SCAN;

int count = 0;
bool armed = false;
int clearRun = 0;
int detectRun = 0;

int motorDir = 1;                                 // +1 clockwise, -1 anti-clockwise
int motorPwm = 0;
unsigned long stopEndMs = 0;

bool imuOk = false;
unsigned long lastImuTryMs = 0;

// ---------- Laser / motor ----------
void laserSet(bool on) {
  digitalWrite(LASER_PIN, on ? HIGH : LOW);
}

void driveMotor(int dir, int speed) {
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
  count = 0;
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (digitalRead(SENSOR_PIN) == LOW) count++;
    delayMicroseconds(100);
  }
}

// ---------- Output ----------
void printLine(bool detected, unsigned long now) {
  Serial.print(F("S1(D"));
  Serial.print(SENSOR_PIN);
  Serial.print(F("): "));
  Serial.print(count);
  Serial.print(F("  "));
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
  if (state == ST_SCAN) {
    Serial.print(F("SCAN "));
    Serial.print(motorDir > 0 ? F("CW") : F("CCW"));
    if (!armed) Serial.print(F(" arming"));
  } else {
    long left = (long)(stopEndMs - now);
    Serial.print(F("STOP "));
    Serial.print((left > 0 ? left : 0) / 1000.0f, 1);
    Serial.print(F("s LASER ON"));
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

void startStop() {
  stopMotor();                                    // motor stops first ...
  laserSet(true);                                 // ... then the laser shoots for the whole stop
  stopEndMs = millis() + STOP_MS;
  state = ST_STOP;
  Serial.println(F(">>> STOP (message on D6) - laser ON <<<"));
}

void endStop() {
  laserSet(false);                                // laser OFF, then spin again
  armed = false;                                  // the signal must clear before the next detection
  clearRun = 0;
  Serial.println(F(">>> STOP finished - laser OFF, scanning <<<"));
  startScan();
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

  pinMode(SENSOR_PIN, INPUT_PULLUP);

  Serial.println(F("=== Spin Detect One v1.0 (sensor D6, laser D10, MPU6050=A4/A5) ==="));
  imuOk = imuInit();
  Serial.println(imuOk ? F("MPU6050 found at 0x68") : F("MPU6050 not found at 0x68 (IMU:OFF) - continuing"));
  Serial.println(F("Auto-starting: scan, detect, stop 4 s with laser ON"));
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

  bool detected = count >= DETECT_THRESHOLD;
  unsigned long now = millis();

  if (state == ST_SCAN) {
    if (!armed) {
      if (detected) {
        clearRun = 0;
      } else if (++clearRun >= CLEAR_WINDOWS_TO_REARM) {
        armed = true;
      }
    } else if (detected) {
      if (++detectRun >= CONFIRM_WINDOWS) startStop();
    } else {
      detectRun = 0;
    }
  }

  printLine(detected, now);
}
