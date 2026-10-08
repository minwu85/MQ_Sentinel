# Spin Detect One (v1.0) – spin_detect with a single sensor

Environment `spin_detect_one` · source [src/spin_detect_one_main.cpp](src/spin_detect_one_main.cpp) · Nano V3 · **115200 baud**.
Everything is the same as [spin_detect](spin_detect.md), but with **one sensor (D6)** on the laser axis. There is nothing to turn toward, so the turn logic is removed: a detection stops the motor, the laser is ON for the whole 4 s stop, then OFF, and scanning resumes.

```
pio run -e spin_detect_one -t upload
pio device monitor -e spin_detect_one
```

> **Status: compiles; not bench tested.**

## 1. Behaviour
```
SCAN (clockwise, laser OFF) ─ D6 count ≥ 5 ─► STOP (motor off, then laser ON, 4 s) ─► laser OFF ─► SCAN
```
* Detection is the `spin_until_ir` method: LOW reads of D6 are counted in 100 ms windows; `count ≥ K = 5` is a detection (`CONFIRM_WINDOWS = 1`; 2 = more noise immunity).
* After every stop (and at power-up) D6 must be quiet for 2 windows (200 ms) before the next detection, so the same target is not detected twice.
* Order at a stop: **motor off first, then laser ON**. At its end: **laser OFF first, then the motor starts**. The laser is driven LOW first in `setup()` and is HIGH only inside the stop.
* No commands, no faults, no latch: it always resumes.

## 2. Wiring
| Signal | Pin |
|---|---|
| Sensor S1 (TSOP4138, active-LOW, `INPUT_PULLUP`) | D6 |
| Motor nSLEEP / DIR / PWM | D7 / D8 / D9 (DIR HIGH = clockwise) |
| Laser (HIGH = ON) | D10 |
| MPU6050 VCC / GND | 5 V / common GND |
| MPU6050 SDA / SCL / AD0 | A4 / A5 / GND (0x68); INT, XDA, XCL not connected |

The MPU6050 is display only; if it is missing the program continues (`IMU:OFF`).

## 3. Maths
```
loop time (one pin) ≈ 4.5 µs read + 100 µs delay + ~3 µs ≈ 108 µs                    (estimate)
N = 100 ms / 108 µs ≈ 925 samples per window,  K = 5 → 0.54 % duty (≈ 0.5 ms LOW)
t_detect ≤ W (100 ms) after the signal reaches the sensor, 2·W worst case
overshoot  Δθ = ω · (t_detect + t_stop)         ω = 180 °/s, 0.15 s + 0.05 s → 36°
STOP_MS = 4000: the last sampling window is shortened to the time left, so the stop ends on time to ≈ 1 ms (+ oscillator tolerance)
```
Geometry: one sensor sees ±45° (β = 90°), so only 90° of the circle is covered at any instant; the worst-case rotation before the first detection is `360° − 90° = 270°` (1.5 s at 180 °/s). The three-sensor `spin_detect` needs at most 30°. Because the single sensor is the laser axis, the laser points within the beam of the sensor (± a fraction of 45°); there is no alignment step in this version.

## 4. Output
One line per 100 ms window:
```
S1(D6): 72  DETECTED | Acc(g) X:-0.68 Y:-0.60 Z:0.51 | Gyro(dps) X:-5.7 Y:2.7 Z:-3.8 | SCAN CW
S1(D6): 0  -- | Acc(g) X:0.04 Y:-0.19 Z:0.97 | Gyro(dps) X:-1.5 Y:0.4 Z:-1.4 | STOP 3.2s LASER ON
```
`S1(D6): n` = LOW reads in the window; `DETECTED` / `--`; Acc in g (±2 g), Gyro in dps (±1000 dps, bias not removed); last field `SCAN CW` (`arming` until D6 has cleared) or `STOP x.xs LASER ON`.

## 5. Settings (top of the file)
| Constant | Default | Meaning |
|---|---|---|
| `SENSOR_PIN` | 6 | detection input |
| `SPIN_SPEED` | 200 | scan PWM (`spin_until_ir` value) |
| `SCAN_CLOCKWISE` | true | scan direction |
| `WINDOW_MS`, `DETECT_THRESHOLD`, `CONFIRM_WINDOWS` | 100, 5, 1 | from `spin_until_ir` |
| `CLEAR_WINDOWS_TO_REARM` | 2 | 200 ms clear before the next detection |
| `STOP_MS` | 4000 | stop time, laser ON |

## 6. Bench check
1. `ir_sensor_test`: D6 count with the beacon on/off; `K = 5` must lie between them.
2. Laser OFF while spinning, ON for the whole stop (`STOP x.xs LASER ON` counts down from 4.0).
3. `pio device monitor -e spin_detect_one --filter time`: `>>> STOP (...)` to `>>> STOP finished` must be 4.0 s.
4. The same target must not retrigger until D6 has been clear for 200 ms.

## 7. Difference from spin_detect
One sensor instead of three, no turn state and no `SENSOR_TURN` table; everything else (window, K, re-arm, 4 s stop, laser rule, output, IMU, 115200 baud) is identical. The earlier encoder/gyro/bearing version of `spin_detect_one` is archived with its maths in [spin_detect_advanced.md](spin_detect_advanced.md).
