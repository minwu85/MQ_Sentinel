# Spin Detect Clock (v0.1) – PID rotation that keeps D6 on the message

Environment `spin_detect_clock` · source [src/spin_detect_clock_main.cpp](src/spin_detect_clock_main.cpp) · Nano V3 · **115200 baud**.
Copy of [spin_detect](spin_detect.md) (left untouched) with one change: the rotation after a detection is a **PID loop** that turns the assembly like a clock hand until **D6 – the centre sensor on the laser axis – is centred on the message**. Scanning, the 100 ms counting window, `K = 5`, the 4 s stop with the laser ON, the laser-OFF-while-moving rule and the Acc/Gyro output are the same as `spin_detect`.

```
pio run -e spin_detect_clock -t upload
pio device monitor -e spin_detect_clock
```

> **Status: compiles; checked only against a simple computer simulation, not bench tested.** The PID gains, the sensor layout and the motor response are assumptions to be tuned with section 8.

---

## 1. Behaviour

```
SCAN (clockwise, laser OFF, 100 ms windows)
  └─ any sensor ≥ K ─► TURN (PID, 50 ms windows)
        ├─ D6 centred AND at its peak for 2 windows ─► STOP (laser ON, 4 s) ─► SCAN
        ├─ message gone for 0.5 s ─► SCAN
        └─ 6 s without success ─► STOP (laser ON, 4 s) ─► SCAN
```
The motor never stops while the message is still off-centre: the PWM is clamped to at least `MIN_TRACK_PWM`, so it keeps turning until D6 is centred (strong error = fast, weak error = slow). The laser is OFF during SCAN and TURN and ON for the whole STOP.

## 2. Sensor layout (configure)

| Sensor | Pin | Role | `SENSOR_WEIGHT` |
|---|---|---|---|
| S1 | D4 | side, anti-clockwise of D6 | −1 |
| S2 | D5 | side, clockwise of D6 | +1 |
| S3 | D6 | **centre, on the laser axis** | 0 |

This is the `directional_ir` arrangement (D4 left, D5 right, D6 centre). If your sensors are wired the other way round, swap the weights (`{+1, −1, 0}`); if the centre is another pin, change `CENTRE_SENSOR`. Wiring of the rest is unchanged: motor D7/D8/D9 (DIR HIGH = clockwise), laser D10, MPU6050 on A4/A5 (AD0 GND, 0x68, display only).

## 3. Measuring the position of the message

With no angle sensor, the position of the message relative to D6 is estimated from the three counts (only counts ≥ the window threshold are used, so noise is ignored):
```
pos = ( Σ weight_i · count_i ) / ( Σ count_i )  =  (D5 − D4) / (D4 + D5 + D6)          −1 … +1
```
`pos > 0`: the message is clockwise of D6; `pos < 0`: anti-clockwise; `pos ≈ 0`: balanced between the sides or only D6 sees it. Because D6 is in the denominator, a strong D6 pulls `pos` toward 0 (a "strong" centre = small error), while side sensors alone give ±1. `pos` is a ratio, not degrees: with overlapping beams it changes smoothly with the angle; with sensors 120° apart it jumps between −1, 0 and +1.

## 4. PID loop

Convention (textbook form): process variable `y = −pos` (where D6 points relative to the message), setpoint `r = 0` (D6 on the message):
```
e = r − y = pos                                   units: fraction (−1 … +1)
I ← clamp( I + e·dt , ±I_MAX )                    reset to 0 when e changes sign (anti-windup)
D = 0.5·D + 0.5·(e − e_prev)/dt                   filtered derivative
u = Kp·e + Ki·I + Kd·D                            PWM units
direction = sign(u)  (u > 0 clockwise, u < 0 anti-clockwise, u = 0 keep current)
PWM = clamp( |u| , MIN_TRACK_PWM , MAX_TRACK_PWM )
```
`dt` = the tracking window = 50 ms (20 updates per second).

| Term | Role here |
|---|---|
| P (`KP = 140`) | error 1.0 → PWM 140, error 0.4 → 56 → clamped to the minimum: big error = fast turn |
| I (`KI = 40`) | removes a small steady offset (D6 sitting slightly to one side); limited to `±I_MAX = 1` → at most ±40 PWM |
| D (`KD = 6`) | brakes when `e` is falling quickly, to limit overshoot: `e` dropping by 1.0 in 0.1 s → D = −10 → −60 PWM |

**Strength (peak) term.** D6's own count is used as the "weak/strong" signal: `peak` = the largest D6 count since this turn started.
* If D6 sees the message and the sides are balanced (`|pos| ≤ 0.20`) but `c6 < 0.8·peak`, D6 has passed its peak: the error becomes `e = −dir · 0.5 · (1 − c6/peak)`, which drives the motor back toward the peak.
* If D6 is still climbing (`c6` rose by more than `RISE_MARGIN = 2` counts since the last window), the assembly keeps going in the same direction at `MIN_TRACK_PWM` (the first tracking window counts as "rising").

**Stop condition** (needs `ALIGN_CONFIRM_WINDOWS = 2` in a row = 100 ms):
```
c6 ≥ 3  AND  c6 + 4 ≥ max(D4, D5)  AND  |pos| ≤ 0.20  AND  c6 ≥ 0.8·peak  AND  c6 not rising
```
i.e. D6 is detecting, is not weaker than the sides, the sides are balanced, and D6 is at its maximum.

## 5. Counting windows

```
loop time ≈ 3 · 4.5 µs + 100 µs + ~3 µs ≈ 117 µs                                    (estimate)
SCAN window 100 ms:  N ≈ 850 samples, K = 5  (0.6 % duty)                          same as spin_until_ir / spin_detect
TURN window  50 ms:  N ≈ 425 samples, K = 3  (0.7 % duty, same sensitivity), margin 4 (scaled from 8)
```
Reaction time: a window decides at its end – up to 100 ms (SCAN) after the signal arrives, then 50 ms per PID step. After stopping, `STOP_MS = 4000` is timed to within ≈ 1 ms plus the oscillator tolerance, as in `spin_detect`.

## 6. Reverse, lost message, timeout
* A direction change stops the motor for `REVERSE_PAUSE_MS = 40 ms` first (current spike protection); this pause is inside one window.
* No sensor ≥ K for `LOST_WINDOWS = 10` windows (0.5 s) → back to SCAN; shorter gaps keep turning the same way.
* `TRACK_TIMEOUT_MS = 6000` → stop here with the laser ON (the motor always resumes after 4 s).

## 7. Output

```
S1(D4): 5  S2(D5): 40  S3(D6): 60  DETECTED | Acc(g) X:-0.68 Y:-0.60 Z:0.51 | Gyro(dps) X:-5.7 Y:2.7 Z:-3.8 | TURN CW pwm130 e:+0.40
```
Same fields as `spin_detect`; the TURN field shows the direction, the PWM and the PID error `e`. `SCAN CW (arming)` and `STOP 3.2s LASER ON` as before.

## 8. Tuning and bench procedure

1. Run `ir_sensor_test` and check the counts of D4/D5/D6 with the beacon; confirm D6 is the middle sensor and that D4/D5 are on the sides you configured (point the beacon at one side: that sensor must be the strongest).
2. **Direction check:** with the beacon off to the clockwise side the TURN field must show `CW`; if it turns away, swap `SENSOR_WEIGHT`.
3. **Min PWM:** the smallest PWM that reliably turns the assembly → `MIN_TRACK_PWM` (110 is a guess).
4. **Gains (manual tuning):** set `KI = KD = 0`, raise `KP` until the assembly just starts to hunt around the message, then use about half of that value. Add `KD` until the overshoot disappears. Add a little `KI` only if D6 settles slightly off-centre.
5. **Stop quality:** watch `e:` approach 0 and the laser spot on the target; adjust `ALIGN_DEADBAND` (smaller = stricter), `PEAK_FRACTION` and `RISE_MARGIN`.
6. Check the stop is 4.0 s: `pio device monitor -e spin_detect_clock --filter time`.

## 9. Simulation check (not hardware)

A simple Python model (cosine beam pattern, ±45°, noise, first-order motor response, the same PID code) gave these residual pointing errors at the stop:

| Layout (D4 / D6 / D5 mounts) | Residual error | Remark |
|---|---|---|
| overlapping: −30° / 0° / +30° | −3° … +3° | `pos` is smooth, PID centres D6 |
| 120° apart: −120° / 0° / +120° | up to ≈ 17° | only D6 sees the message near its beam edge; the peak climb limits the error |

Accuracy therefore depends on how much the beams overlap; sensors close together centre much better than sensors spread 120° apart. These numbers come from the model, not from your hardware.

## 10. What is the same / different from spin_detect

| | `spin_detect` | `spin_detect_clock` |
|---|---|---|
| Scan, K = 5, 100 ms window, re-arm | same | same |
| Stop 4 s, laser ON only then, laser OFF before moving | same | same |
| Output and IMU | same | same + PID `e` in the TURN field |
| Centre sensor | S1 (D4) | **D6** |
| Turn rule | fixed direction per sensor until the centre is the strongest | **PID on `pos` + D6 peak** until D6 is centred |
| Turn speed | fixed 150 | 110 … 200 from the PID output |
| Tracking window | 100 ms | 50 ms |
