# Spin Detect Clock – code explanation (current version v0.3)

Environment `spin_detect_clock` · source [src/spin_detect_clock_main.cpp](src/spin_detect_clock_main.cpp) · Nano V3 · **115200 baud**.
This is the **code document** (what each part of the program does, with the maths it uses). The **overview document** – goal, what was done, problems and the flowchart/maths of every version – is [spin_detect_clock_process.md](spin_detect_clock_process.md).
`spin_detect` ([spin_detect.md](spin_detect.md)) is a separate program and is not changed.

```
pio run -e spin_detect_clock -t upload
pio device monitor -e spin_detect_clock
```

> **Status: compiles; checked only with a computer simulation, not bench tested.** Gains, sensor layout and motor response are assumptions – tune with section 8.

## 1. What the program does

The motor scans clockwise with the laser OFF. When a sensor sees the message, a PID loop turns the assembly like a clock hand until **D6** (the centre sensor, on the laser axis) is centred on the message, then it stops fast; the laser is ON for the whole 4 s stop, then OFF and scanning resumes. Sensor counts are read in 10 ms slices and a decision is made every slice.

## 2. Wiring

| Signal | Pin |
|---|---|
| S1 / S2 / S3 = D4 / D5 / D6 (TSOP4138, active-LOW, `INPUT_PULLUP`) | D4 = anti-clockwise side, D5 = clockwise side, D6 = centre (laser axis) |
| Motor nSLEEP / DIR / PWM | D7 / D8 / D9 (DIR HIGH = clockwise) |
| Laser (HIGH = ON) | D10 |
| MPU6050 VCC, GND (common), SDA, SCL, AD0, INT/XDA/XCL | 5 V, GND, A4, A5, GND (0x68), not connected – display only |

`SENSOR_WEIGHT = {-1, +1, 0}` and `CENTRE_SENSOR = 2` describe this layout; swap the weights if the sides are the other way round.

## 3. Program structure

| Function | What it does |
|---|---|
| `setup()` | laser LOW first, motor driver awake, sensors `INPUT_PULLUP`, MPU6050 check, start scanning |
| `loop()` | one 10 ms slice per pass: `sampleSlice` → state step (`SCAN` / `TURN` / `STOP`) → print every 100 ms |
| `sampleSlice(ms)` | counts LOW reads of the 3 sensors for one slice, stores it in a ring of 10 slices and computes `c100` (last 100 ms) and `c50` (last 50 ms) |
| `strongest(c)` | index of the largest count |
| `positionOf(c, thr)` | message position `pos` from −1 to +1 relative to D6 (centroid of the sensor weights) |
| `startScan()` | laser OFF, then motor clockwise at `SPIN_SPEED` |
| `startTurn()` | resets the PID and stop memory, picks the first direction from `pos` |
| `turnStep(now)` | the heart of v0.3: stop test → PID → motor command, every slice |
| `startStop(why)` | motor off first, then laser ON, 4 s timer |
| `endStop()` | laser OFF, re-arm required, scan again |
| `driveMotor`, `stopMotor`, `laserSet` | the only places that touch the motor and laser pins |
| `imuInit`, `imuRead`, `printLine` | MPU6050 and the output line |

State machine:
```
SCAN ──(armed and rolling 100 ms count ≥ 5)──► TURN ──(stop test passes 3 slices | 1.8 s limit)──► STOP (4 s, laser ON) ──► SCAN
                                                 └──(no signal 0.5 s | 2.5 s safety net)──► SCAN
```

## 4. Maths used in the code

**Counting.** Loop time ≈ 3 · 4.5 µs + 100 µs + ~3 µs ≈ 117 µs (estimate), so a 10 ms slice holds ≈ 85 reads per sensor.
```
c100[i] = Σ last 10 slices   (100 ms)     scan detection: max(c100) ≥ 5  (spin_until_ir value, ≈ 0.6 % duty)
c50[i]  = Σ last  5 slices   ( 50 ms)     turn / stop:    counts ≥ 3 are used (same duty)
```
**Position (the clock error).**
```
pos = Σ weight_i · c50_i / Σ c50_i = (D5 − D4) / (D4 + D5 + D6)          only counts ≥ 3; −1 … +1; + = message clockwise of D6
setpoint = 0 (message centred on D6),  error e = pos
```
Noise on `pos`: counts are roughly Poisson, so `σ_pos ≈ √(D4 + D5) / (D4 + D5 + D6)`; for D4 = D5 = 12, D6 = 25 → σ ≈ 0.10.

**PID.**
```
I ← clamp(I + e·dt, ±1), reset when e changes sign          D = 0.8·D + 0.2·(e − e_prev)/dt          dt ≈ 10 ms
u = Kp·e + Ki·I + Kd·D          Kp = 140, Ki = 40, Kd = 6
direction = sign(u)  (+ clockwise)          PWM = clamp(|u|, 110, 200)   → big error fast, small error slow, never below 110
```
**Stop test (new in v0.3).** Every slice, while a signal is present (some count ≥ 3):
```
dead(t) = 0.15 + 0.45 · min(1, t / 1.2 s)           dead band widens from 0.15 to 0.60 in 1.2 s
crossed = pos changed sign (|pos| ≥ 0.05) since the turn began      the message passed the centre
centred = |pos| ≤ dead(t)  OR  crossed
stop when `centred` has held for 3 slices (30 ms);   hard limit: stop at 1.8 s if the message is still on the sensors
```
Why it works: at t = 0 the dead band (0.15) is about 1.5 σ of the noise, so a good alignment stops almost immediately; if noise or a coarse layout keeps `|pos|` above it, the band reaches 2 σ at ≈ 0.3 s and 6 σ at 1.2 s, so the condition is bound to be true well before the 1.8 s limit. Nothing here needs D6 itself to be strong, only that the sensors see the message.

**Timing.** Slice 10 ms, stop confirmation 30 ms, decision latency ≤ 10 ms + 50 ms of integration. Stop time `STOP_MS = 4000` is timed to ≈ 1 ms (the last slice is shortened) plus the oscillator tolerance. Overshoot after the stop command: `ω · t_brake`; at ω = 64 °/s (PWM 110, assumed) and 50 ms → 3°.

## 5. Constants (top of the file)

| Constant | Default | Meaning |
|---|---|---|
| `SENSOR_PINS`, `SENSOR_WEIGHT`, `CENTRE_SENSOR` | D4 D5 D6, `{-1,+1,0}`, 2 | layout |
| `SPIN_SPEED`, `SCAN_CLOCKWISE` | 200, true | scan PWM / direction |
| `SLICE_MS`, `RING`, `FAST_SLICES` | 10 ms, 10, 5 | counting slices and rolling windows |
| `DETECT_THRESHOLD`, `TRACK_THRESHOLD` | 5 per 100 ms, 3 per 50 ms | signal thresholds |
| `CLEAR_SLICES_TO_REARM` | 20 | 200 ms without signal before the next detection |
| `KP`, `KI`, `KD`, `I_MAX` | 140, 40, 6, 1 | PID |
| `MIN_TRACK_PWM`, `MAX_TRACK_PWM` | 110, 200 | smallest PWM that still turns / limit |
| `DEAD_START`, `DEAD_END`, `RELAX_MS` | 0.15, 0.60, 1200 | dead band |
| `SIGN_MIN`, `STOP_SLICES` | 0.05, 3 | zero-crossing sensitivity, confirmation slices |
| `FORCE_STOP_MS`, `TRACK_TIMEOUT_MS`, `LOST_SLICES` | 1800, 2500, 50 | hard stop, safety net, lost message |
| `STOP_MS` | 4000 | stop time with the laser ON |
| `REVERSE_PAUSE_MS` | 30 | pause before the motor reverses |

## 6. Output
Every 100 ms:
```
S1(D4): 5  S2(D5): 40  S3(D6): 60  DETECTED | Acc(g) X:-0.68 Y:-0.60 Z:0.51 | Gyro(dps) X:-5.7 Y:2.7 Z:-3.8 | TURN CW pwm130 e:+0.40
```
`S1(D4): n` = LOW reads in the last 100 ms; `DETECTED` / `--`; Acc in g (±2 g), Gyro in dps (±1000 dps, bias not removed); last field `SCAN CW` (`arming` until clear), `TURN CW/CCW pwm… e:…` (PID error), or `STOP x.xs LASER ON`. Event lines mark each state change, including `>>> STOP (D6 centred on the message) - laser ON <<<` and the D6 warning.

## 7. Laser rule
The laser (D10) is driven LOW first in `setup()`, is OFF in `SCAN` and `TURN`, and HIGH only inside `STOP`. Order at a stop: motor off, then laser ON; at its end: laser OFF, then the motor starts.

## 8. Tuning and bench checks
1. `ir_sensor_test`: D4/D5 on the sides you configured, D6 in the middle; D6 gets counts when the beacon is on its axis.
2. Direction: beacon on the clockwise side → TURN field shows `CW` and D6 moves toward it; otherwise swap `SENSOR_WEIGHT`.
3. `MIN_TRACK_PWM`: smallest PWM that reliably turns the assembly.
4. Stop: the time between `>>> message on S…` and `>>> STOP` should be well under 1.8 s (`pio device monitor -e spin_detect_clock --filter time`). If it often runs into the 1.8 s limit, the beam geometry or `MIN_TRACK_PWM` is the cause – raise `DEAD_START` or lower `RELAX_MS`.
5. Aim: if the laser lands off the target, lower `DEAD_START` (tighter) – the dead band still widens if it cannot settle.
6. `KP`, `KD`: reduce `KP` if it overshoots; `KI` only if it settles on one side.
