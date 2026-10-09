# 3-Sensor IR Directional Motor Test

## Overview

This program uses three IR sensors to determine whether an object is detected on the **left, centre, or right**. The Arduino Nano reads the three sensors for approximately 50 ms, counts how many times each sensor detects an IR signal, and compares the detection counts. Based on the strongest signal, the Arduino controls a DRV8874 motor driver. If no IR signal is detected, the motor stops. If the centre sensor has the strongest signal, the motor also stops. If the left sensor has the stronger signal, the motor turns in one direction, while a stronger right signal makes the motor turn in the opposite direction.

---

## 1. Pin Definitions

```cpp
const int IR_LEFT   = 4;
const int IR_RIGHT  = 5;
const int IR_CENTRE = 6;

const int MOTOR_DIR = 8;   // DRV8874 IN2 / PH
const int MOTOR_PWM = 9;   // DRV8874 IN1 / EN
```

The program assigns names to the Arduino pins connected to the three IR sensors and the motor driver. Pins 4, 5, and 6 are used as inputs for the left, right, and centre IR sensors. Pin 8 controls the motor direction through the DRV8874, while pin 9 provides the PWM signal used to control motor speed.

| Component | Arduino Pin | Purpose |
|---|---:|---|
| Left IR sensor | 4 | Detects IR on the left |
| Right IR sensor | 5 | Detects IR on the right |
| Centre IR sensor | 6 | Detects IR in the centre |
| Motor direction | 8 | Controls motor direction |
| Motor PWM | 9 | Controls motor speed |

---

## 2. `setup()` Function

```cpp
void setup() {
  Serial.begin(9600);

  pinMode(IR_LEFT, INPUT_PULLUP);
  pinMode(IR_RIGHT, INPUT_PULLUP);
  pinMode(IR_CENTRE, INPUT_PULLUP);

  pinMode(MOTOR_DIR, OUTPUT);
  pinMode(MOTOR_PWM, OUTPUT);

  analogWrite(MOTOR_PWM, 0);

  Serial.println("3-sensor directional test ready");
}
```

The `setup()` function runs once when the Arduino starts. It begins Serial communication at 9600 baud so that sensor and motor decisions can be displayed in the Serial Monitor. The three IR sensors are configured as inputs using the Arduino's internal pull-up resistors, while the motor direction and PWM pins are configured as outputs. The PWM is initially set to `0`, ensuring that the motor remains stopped when the system first starts.

The program assumes that an IR sensor outputs **LOW when IR is detected** and **HIGH when nothing is detected**.

---

## 3. Sensor Counters

```cpp
int leftCount = 0;
int rightCount = 0;
int centreCount = 0;
```

At the beginning of every loop, three counters are created and set to zero. These counters record how many times each sensor detects an IR signal during the sampling period. Instead of making a decision from a single sensor reading, the program takes multiple readings over approximately 50 ms. This makes the detection decision more stable.

---

## 4. Sampling the Sensors

```cpp
unsigned long start = millis();

while (millis() - start < 50) {
  if (digitalRead(IR_LEFT) == LOW) {
    leftCount++;
  }

  if (digitalRead(IR_RIGHT) == LOW) {
    rightCount++;
  }

  if (digitalRead(IR_CENTRE) == LOW) {
    centreCount++;
  }

  delayMicroseconds(200);
}
```

The program records the current time using `millis()` and then continuously reads all three IR sensors for approximately 50 ms. Whenever a sensor returns `LOW`, its corresponding counter is increased. The `delayMicroseconds(200)` introduces a very short delay between readings. At the end of the 50 ms sampling period, the three counters represent the relative strength of the IR detection from each sensor.

For example:

```text
Left sensor:      170 detections
Centre sensor:     20 detections
Right sensor:      10 detections
```

This indicates that the IR signal is strongest on the left.

---

## 5. No IR Detected — Stop

```cpp
if (leftCount == 0 && rightCount == 0 && centreCount == 0) {
  analogWrite(MOTOR_PWM, 0);
  Serial.println("NO IR - STOP");
}
```

If all three counters are zero, none of the sensors detected an IR signal during the 50 ms sampling period. The motor PWM is therefore set to zero, stopping the motor. The Serial Monitor displays `NO IR - STOP`.

---

## 6. Centre IR Signal — Stop

```cpp
else if (centreCount >= leftCount && centreCount >= rightCount) {
  analogWrite(MOTOR_PWM, 0);
  Serial.println("CENTRE - STOP");
}
```

If the centre sensor has the highest detection count, the motor stops. This means the target or IR source is considered to be directly in front of the centre sensor. The `>=` operator also means that the centre sensor wins if there is a tie between the centre and another sensor.

For example:

```text
Left:     30
Centre:  150
Right:    20

Result: CENTRE - STOP
```

---

## 7. Left IR Signal — Turn Left

```cpp
else if (leftCount > rightCount) {
  digitalWrite(MOTOR_DIR, LOW);
  analogWrite(MOTOR_PWM, 140);
  Serial.println("LEFT - TURN");
}
```

If the centre sensor is not the strongest and the left sensor has a greater detection count than the right sensor, the program selects the left direction. The motor direction pin is set to `LOW`, and the PWM is set to `140`. Since the maximum Arduino PWM value is 255, a value of 140 represents approximately 55% PWM.

```text
PWM = 140 / 255
    ≈ 55%
```

The Serial Monitor displays `LEFT - TURN`.

---

## 8. Right IR Signal — Turn Right

```cpp
else {
  digitalWrite(MOTOR_DIR, HIGH);
  analogWrite(MOTOR_PWM, 140);
  Serial.println("RIGHT - TURN");
}
```

If none of the previous conditions are true, the program assumes that the right sensor has the strongest IR signal. The motor direction pin is set to `HIGH`, and the PWM is set to `140`, causing the motor to turn in the opposite direction from the left case.

The Serial Monitor displays `RIGHT - TURN`.

---

## 9. Decision Logic

The program's main decision-making process can be represented as follows:

```text
                    Read 3 sensors
                          │
                          ▼
                  Count detections
                          │
                          ▼
              ┌───────────┴───────────┐
              │                       │
           All = 0?               Not all = 0
              │                       │
             YES                      NO
              │                       │
              ▼                       ▼
            STOP              Is CENTRE strongest?
                                      │
                         ┌────────────┴────────────┐
                         │                         │
                        YES                       NO
                         │                         │
                         ▼                         ▼
                       STOP              Is LEFT > RIGHT?
                                                  │
                                      ┌───────────┴───────────┐
                                      │                       │
                                     YES                     NO
                                      │                       │
                                      ▼                       ▼
                                LEFT TURN               RIGHT TURN
```

---

## 10. Complete System Flow

The overall operation of the program is:

```text
                 Arduino starts
                       │
                       ▼
                 setup() runs
                       │
                       ▼
              Configure IR sensors
                       │
                       ▼
             Configure motor driver
                       │
                       ▼
                Motor starts STOP
                       │
                       ▼
                 loop() begins
                       │
                       ▼
                Read 3 IR sensors
                       │
                       ▼
              Sample for 50 ms
                       │
                       ▼
             Count IR detections
                       │
                       ▼
             Compare three counts
                       │
          ┌────────────┼────────────┐
          │            │            │
          ▼            ▼            ▼
       Nothing      Centre       Left/Right
       detected     strongest      strongest
          │            │            │
          ▼            ▼       ┌────┴────┐
        STOP         STOP      │         │
                               ▼         ▼
                           LEFT TURN  RIGHT TURN
                               │         │
                               └────┬────┘
                                    │
                                    ▼
                              Repeat loop()
```

---

## 11. Example Sensor Decisions

### Example 1 — No IR

```text
Left   = 0
Centre = 0
Right  = 0
```

Result:

```text
NO IR - STOP
```

The motor stops.

---

### Example 2 — IR source in the centre

```text
Left   = 20
Centre = 180
Right  = 25
```

Result:

```text
CENTRE - STOP
```

The motor stops because the centre sensor has the strongest signal.

---

### Example 3 — IR source on the left

```text
Left   = 170
Centre = 20
Right  = 30
```

Result:

```text
LEFT - TURN
```

The motor direction is set to `LOW` and PWM is set to `140`.

---

### Example 4 — IR source on the right

```text
Left   = 20
Centre = 30
Right  = 170
```

Result:

```text
RIGHT - TURN
```

The motor direction is set to `HIGH` and PWM is set to `140`.

---

## 12. Summary

This program demonstrates a basic **three-sensor IR directional control system**. The Arduino continuously samples the left, centre, and right IR sensors, counts their detections, and determines where the strongest IR signal is located. The motor is stopped when there is no IR signal or when the centre sensor has the strongest signal. When the strongest signal is on the left or right, the Arduino changes the motor direction and applies a PWM value of 140 to turn the motor.

The main control relationship is:

```text
IR sensors
    ↓
Sensor readings
    ↓
Detection counts
    ↓
Compare LEFT / CENTRE / RIGHT
    ↓
┌──────────┬──────────┬──────────┐
│          │          │
LEFT     CENTRE     RIGHT
│          │          │
▼          ▼          ▼
TURN       STOP       TURN
```

This provides the basic sensor-to-motor logic that can later be expanded into a more complete autonomous control system.