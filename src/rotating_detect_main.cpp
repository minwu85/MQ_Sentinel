// ============================================================
// ROTATING DETECTION SYSTEM (v0.5) - 3x TSOP4138 @ 120 deg - Nano V3
// Build/upload with: pio run -e rotating_detect -t upload
// Then:              pio device monitor -e rotating_detect
//
// The assembly rotates continuously about a fixed centre. Three IR
// receivers sit 120 deg apart (S1=D4 @ 0, S2=D5 @ 120, S3=D6 @ 240).
// Each receiver sees +/-45 deg, so together they cover 270 deg of the
// circle at any instant; rotation sweeps the remaining 30 deg gaps, so
// every bearing enters a beam within 30 deg of rotation.
//
// Cycle:  ROTATING -> (confirmed detection) motor stops immediately
//         -> ALIGN (optional: turn so the laser points at the target)
//         -> INDICATE (red laser pulses briefly, then off)
//         -> PAUSE -> ROTATING again (or wait for 'g')
//         any fault  -> FAULT (motor stopped, laser off, driver asleep)
//
// A detection is only accepted when ALL of these hold:
//   1. the system is armed (all sensors were clear for REARM windows)
//   2. exactly ONE sensor reports >= DETECT_K LOW reads in a window
//   3. that same sensor repeats it for CONFIRM_WINDOWS windows in a row
// Faults: no detection timeout, encoder stall, sensor stuck LOW,
// several sensors firing at once (flooding/noise), align timeout.
//
// Laser is a visual marker only: it is OFF while rotating, pulses
// LASER_PULSE_MS after a stop, and is hard-capped at LASER_MAX_MS.
//
// Serial commands: r = reset fault / restart, g = resume (when
// AUTO_RESUME is false), z = zero position, p = print status.
// Maths and tuning: IR_Spin_Stop_Math.md (section v0.5)
// ============================================================
#include <Arduino.h>
#include <math.h>

// ---------- Pins ----------
const int NUM_SENSORS = 3;
const int SENSOR_PINS[NUM_SENSORS] = {4, 5, 6};
const float SENSOR_MOUNT_DEG[NUM_SENSORS] = {0.0f, 120.0f, 240.0f}; // 360 / 3
const char *SENSOR_NAME[NUM_SENSORS] = {"S1", "S2", "S3"};

const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN   = 8;
const int MOTOR_PWM_PIN   = 9;
const int LASER_PIN       = 10;
const int ENCODER_A_PIN   = 2;
const int ENCODER_B_PIN   = 3;

// ---------- Geometry / calibration ----------
// Encoder counts for ONE full turn of the rotating assembly (after any
// gearbox). 0 = not calibrated: positions are shown in counts only and
// laser alignment is skipped. Measure it by turning the assembly one full
// turn by hand and reading the count with 'p'.
const long COUNTS_PER_REV = 0;
const float LASER_MOUNT_DEG = 0.0f;   // laser axis angle on the assembly (S1 axis = 0)
const bool ALIGN_LASER = true;        // turn laser onto the detected sensor's axis before firing

// ---------- Motor ----------
const int ROTATE_SPEED = 150;         // PWM 0-255
const int ALIGN_SPEED  = 110;
const unsigned long STALL_GRACE_MS = 600;
const unsigned long STALL_CHECK_MS = 500;
const long STALL_MIN_COUNTS = 3;      // fewer counts than this in STALL_CHECK_MS = stalled

// ---------- Detection / debounce ----------
const unsigned long SAMPLE_PERIOD_US = 100;   // one pin read every 100 us
const unsigned long WINDOW_MS = 20;           // 20 ms / 100 us = ~200 samples per window
const int DETECT_K = 20;                      // LOW reads (of ~200) = "clearly in the beam"
const int CONFIRM_WINDOWS = 3;                // same sensor, consecutive windows
const int REARM_CLEAR_WINDOWS = 5;            // all sensors quiet before next detection
const int MAX_MULTI_WINDOWS = 10;             // consecutive multi-sensor windows -> fault
const unsigned long STUCK_MS = 3000;          // one sensor solid LOW this long -> fault

// ---------- Timing ----------
const unsigned long ROTATE_TIMEOUT_MS = 30000; // no valid detection for this long -> fault
const int MAX_REVS_NO_DETECT = 5;              // also checked when COUNTS_PER_REV is set
const unsigned long LASER_PULSE_MS = 300;
const unsigned long LASER_MAX_MS = 1000;       // hard safety cap on any laser-on time
const unsigned long CYCLE_PAUSE_MS = 1000;
const bool AUTO_RESUME = true;
const float ALIGN_TOL_DEG = 3.0f;
const unsigned long ALIGN_TIMEOUT_MS = 4000;

// ---------- State ----------
enum State { ST_ROTATING, ST_ALIGN, ST_INDICATE, ST_PAUSE, ST_FAULT };
enum Fault { F_NONE, F_TIMEOUT, F_ENCODER_STALL, F_SENSOR_STUCK, F_MULTI_SENSOR, F_ALIGN_TIMEOUT };

State state = ST_ROTATING;
Fault fault = F_NONE;
unsigned long stateSinceMs = 0;

volatile long encoderCount = 0;
int rotateSign = 1;          // +1 if DIR=HIGH gives rising counts; learned during the first spin
bool signLearned = false;

// sampling window
unsigned long lastSampleUs = 0;
unsigned long windowStartMs = 0;
uint16_t counts[NUM_SENSORS];
uint16_t samples = 0;

// detection bookkeeping
bool armed = false;
int clearRun = 0;
int candidate = -1;
int confirmRun = 0;
int multiRun = 0;
unsigned long lowRunMs[NUM_SENSORS];
long candidateStartCounts = 0;

// rotation bookkeeping
unsigned long rotateStartMs = 0;
long rotateStartCounts = 0;
unsigned long stallArmedAtMs = 0;
bool stallRefValid = false;
unsigned long stallRefMs = 0;
long stallRefCounts = 0;

// detection result
int detSensor = -1;
long detStopCounts = 0;
long alignStartCounts = 0;
float alignOffsetDeg = 0.0f;

// laser
bool laserOn = false;
unsigned long laserOnSinceMs = 0;

// ---------- Helpers ----------
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

// Unwrapped angle in the rotate-positive direction; NAN if not calibrated.
float countsToDeg(long c) {
  if (COUNTS_PER_REV <= 0) return NAN;
  return (float)rotateSign * (float)c * 360.0f / (float)COUNTS_PER_REV;
}

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

void printPos(long c) {
  Serial.print(c);
  Serial.print(" cnt");
  if (COUNTS_PER_REV > 0) {
    Serial.print(" (");
    Serial.print(wrap360(countsToDeg(c)), 1);
    Serial.print(" deg)");
  }
}

void setLaser(bool on) {
  digitalWrite(LASER_PIN, on ? HIGH : LOW);
  if (on && !laserOn) laserOnSinceMs = millis();
  laserOn = on;
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
}

const char *faultName(Fault f) {
  switch (f) {
    case F_TIMEOUT:       return "TIMEOUT (no valid detection)";
    case F_ENCODER_STALL: return "ENCODER_STALL (motor not turning)";
    case F_SENSOR_STUCK:  return "SENSOR_STUCK (sensor solid LOW)";
    case F_MULTI_SENSOR:  return "MULTI_SENSOR (several sensors at once)";
    case F_ALIGN_TIMEOUT: return "ALIGN_TIMEOUT (laser alignment failed)";
    default:              return "NONE";
  }
}

void enterFault(Fault f) {
  motorStop();
  setLaser(false);
  digitalWrite(MOTOR_SLEEP_PIN, LOW);
  fault = f;
  state = ST_FAULT;
  stateSinceMs = millis();
  Serial.print("!!! FAULT: ");
  Serial.println(faultName(f));
  Serial.println("Send 'r' to reset.");
}

void enterRotating() {
  unsigned long now = millis();
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);
  setLaser(false);
  fault = F_NONE;
  armed = false;
  clearRun = 0;
  candidate = -1;
  confirmRun = 0;
  multiRun = 0;
  for (int i = 0; i < NUM_SENSORS; i++) lowRunMs[i] = 0;
  resetWindow(now);
  rotateStartMs = now;
  rotateStartCounts = readCounts();
  stallRefValid = false;
  stallArmedAtMs = now + STALL_GRACE_MS;
  motorRun(true, ROTATE_SPEED);
  state = ST_ROTATING;
  stateSinceMs = now;
  Serial.println(">>> ROTATING <<<");
}

void enterIndicate() {
  setLaser(true);
  state = ST_INDICATE;
  stateSinceMs = millis();
  Serial.println(">>> LASER MARK <<<");
}

void enterAlign() {
  alignOffsetDeg = wrap180(SENSOR_MOUNT_DEG[detSensor] - LASER_MOUNT_DEG);
  if (!ALIGN_LASER || COUNTS_PER_REV <= 0 || fabsf(alignOffsetDeg) <= ALIGN_TOL_DEG) {
    enterIndicate();
    return;
  }
  alignStartCounts = readCounts();
  motorRun(alignOffsetDeg > 0, ALIGN_SPEED);
  state = ST_ALIGN;
  stateSinceMs = millis();
  Serial.print(">>> ALIGN LASER by ");
  Serial.print(alignOffsetDeg, 1);
  Serial.println(" deg <<<");
}

void acceptDetection(int sensor) {
  motorStop(); // stop immediately, before any printing
  detSensor = sensor;
  detStopCounts = readCounts();

  Serial.print("DETECT ");
  Serial.print(SENSOR_NAME[sensor]);
  Serial.print(" mount=");
  Serial.print(SENSOR_MOUNT_DEG[sensor], 0);
  Serial.print(" first_seen=");
  printPos(candidateStartCounts);
  Serial.print(" stopped=");
  printPos(detStopCounts);
  if (COUNTS_PER_REV > 0) {
    Serial.print(" bearing_est=");
    Serial.print(wrap360(countsToDeg(detStopCounts) + SENSOR_MOUNT_DEG[sensor]), 1);
    Serial.print(" deg (+/-45)");
  }
  Serial.println();

  enterAlign();
}

// ---------- Per-window evaluation ----------
void evaluateWindow() {
  bool hit[NUM_SENSORS];
  int nHit = 0, idx = -1;

  for (int i = 0; i < NUM_SENSORS; i++) {
    hit[i] = counts[i] >= DETECT_K;
    if (hit[i]) { nHit++; idx = i; }

    // sensor validation: solid LOW for too long means stuck / flooded
    if (samples > 0 && (unsigned long)counts[i] * 20UL >= (unsigned long)samples * 19UL) {
      lowRunMs[i] += WINDOW_MS;
      if (lowRunMs[i] >= STUCK_MS) { enterFault(F_SENSOR_STUCK); return; }
    } else {
      lowRunMs[i] = 0;
    }
  }

  if (!armed) {
    clearRun = (nHit == 0) ? clearRun + 1 : 0;
    if (clearRun >= REARM_CLEAR_WINDOWS) {
      armed = true;
      Serial.println("armed");
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
    candidateStartCounts = readCounts();
  }

  if (confirmRun >= CONFIRM_WINDOWS) {
    acceptDetection(idx);
  }
}

void checkStall(unsigned long now) {
  if ((long)(now - stallArmedAtMs) < 0) return;
  if (!stallRefValid) {
    stallRefValid = true;
    stallRefMs = now;
    stallRefCounts = readCounts();
    return;
  }
  if (now - stallRefMs < STALL_CHECK_MS) return;

  long c = readCounts();
  long d = c - stallRefCounts;
  if (labs(d) < STALL_MIN_COUNTS) {
    enterFault(F_ENCODER_STALL);
    return;
  }
  if (!signLearned) {
    rotateSign = (d > 0) ? 1 : -1;
    signLearned = true;
    Serial.print("encoder direction learned: sign=");
    Serial.println(rotateSign);
  }
  stallRefMs = now;
  stallRefCounts = c;
}

void printStatus() {
  Serial.print("state=");
  Serial.print((int)state);
  Serial.print(" pos=");
  printPos(readCounts());
  Serial.print(" armed=");
  Serial.print(armed ? 1 : 0);
  Serial.print(" fault=");
  Serial.println(faultName(fault));
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') {
      enterRotating();
    } else if (c == 'g' && state == ST_PAUSE) {
      enterRotating();
    } else if (c == 'z') {
      noInterrupts();
      encoderCount = 0;
      interrupts();
      Serial.println("position zeroed");
    } else if (c == 'p') {
      printStatus();
    }
  }
}

// ---------- Arduino entry points ----------
void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW); // never power up firing
  analogWrite(MOTOR_PWM_PIN, 0);

  for (int i = 0; i < NUM_SENSORS; i++) pinMode(SENSOR_PINS[i], INPUT_PULLUP);
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);

  Serial.println("=== Rotating Detection v0.5 (S1=D4@0, S2=D5@120, S3=D6@240, laser=D10) ===");
  if (COUNTS_PER_REV <= 0) {
    Serial.println("WARNING: COUNTS_PER_REV = 0 -> positions in counts, laser alignment skipped.");
  }
  Serial.println("Commands: r reset | g resume | z zero | p status");
  delay(1000);
  enterRotating();
}

void loop() {
  unsigned long now = millis();

  handleSerial();

  if (laserOn && now - laserOnSinceMs > LASER_MAX_MS) {
    setLaser(false);
  }

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

      checkStall(now);
      if (state != ST_ROTATING) break;

      bool timedOut = (now - rotateStartMs > ROTATE_TIMEOUT_MS);
      if (COUNTS_PER_REV > 0 && !timedOut) {
        float turned = fabsf(countsToDeg(readCounts() - rotateStartCounts));
        timedOut = turned > 360.0f * MAX_REVS_NO_DETECT;
      }
      if (timedOut) enterFault(F_TIMEOUT);
      break;
    }

    case ST_ALIGN: {
      float turned = countsToDeg(readCounts() - alignStartCounts);
      float progress = (alignOffsetDeg > 0) ? turned : -turned; // + = toward target
      float remaining = fabsf(alignOffsetDeg) - progress;
      bool wrongWay = progress < -ALIGN_TOL_DEG; // spinning away from target
      if (remaining <= ALIGN_TOL_DEG && !wrongWay) {
        motorStop();
        enterIndicate();
      } else if (now - stateSinceMs > ALIGN_TIMEOUT_MS || wrongWay) {
        enterFault(F_ALIGN_TIMEOUT);
      }
      break;
    }

    case ST_INDICATE:
      if (now - stateSinceMs >= LASER_PULSE_MS) {
        setLaser(false);
        state = ST_PAUSE;
        stateSinceMs = now;
        Serial.println(">>> LASER OFF - pause <<<");
      }
      break;

    case ST_PAUSE:
      if (AUTO_RESUME && now - stateSinceMs >= CYCLE_PAUSE_MS) {
        enterRotating();
      }
      break;

    case ST_FAULT:
      break;
  }
}
