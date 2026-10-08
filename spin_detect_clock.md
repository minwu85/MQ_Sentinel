# Spin Detect Clock (v0.2) – compare the sensors, turn D6 to the greatest, stop at once

Environment `spin_detect_clock` · source [src/spin_detect_clock_main.cpp](src/spin_detect_clock_main.cpp) · Nano V3 · **115200 baud**.
Copy of [spin_detect](spin_detect.md) (left untouched) with a new rotation and a new, much faster stop. Scanning, `K = 5` per 100 ms, the 4 s stop with the laser ON, the laser-OFF-while-moving rule and the Acc/Gyro output are the same as `spin_detect`.

```
pio run -e spin_detect_clock -t upload
pio device monitor -e spin_detect_clock
```

> **Status: compiles; checked only against a simple computer simulation, not bench tested.** Gains, sensor layout and motor response are assumptions – tune with section 9.

## 1. What was wrong in v0.1 and what changed

v0.1 only stopped when *five* conditions held at the same time (D6 detecting, not weaker than the sides, sides balanced, at its peak, not rising) and checked them every 50–100 ms; they rarely held together, so it kept "determining" until the 6 s timeout, then stopped and fired the laser. v0.2:

| | v0.1 | v0.2 |
|---|---|---|
| Stop rule | 5 conditions, 2 windows of 50 ms | **D6 is the greatest** – one comparison, 3 slices (30 ms) |
| Decision rate | every 50 ms (20 Hz) | **every 10 ms (100 Hz)** from a rolling 50 ms count |
| Direction | sign of a weighted position | **toward the greatest sensor** (D4 → anti-clockwise, D5 → clockwise) |
| PID error | weighted position, peak terms | **difference between the greatest side sensor and D6** |
| Timeout (D6 never greatest) | stop + laser | **no stop, no laser** – back to scanning |
| D6 check | none | warning if D6 never received the message |

## 2. Behaviour
```
SCAN (clockwise, laser OFF)          rolling 100 ms count ≥ 5 on any sensor
  └─► TURN   every 10 ms: compare the sensors (last 50 ms)
        ├─ D6 is the greatest for 3 slices (30 ms) ─► STOP (motor off, then laser ON, 4 s) ─► laser OFF ─► SCAN
        ├─ a side sensor is greater ─► turn D6 toward it (PID sets the speed)
        ├─ no signal for 0.5 s ─► SCAN
        └─ 4 s and D6 never greatest ─► SCAN (no stop, no laser)
```
The laser is OFF during SCAN and TURN and ON for the whole stop. The motor keeps turning (PWM never below `MIN_TRACK_PWM`) until D6 is the greatest.

## 3. Sensor layout (configure)
| Sensor | Pin | Role | `SENSOR_WEIGHT` |
|---|---|---|---|
| S1 | D4 | side, anti-clockwise of D6 | −1 |
| S2 | D5 | side, clockwise of D6 | +1 |
| S3 | D6 | **centre, on the laser axis** | 0 |

(`directional_ir` arrangement.) If the sides are swapped, use `{+1, −1, 0}`; if the centre is another pin, change `CENTRE_SENSOR`. Rest of the wiring is unchanged: motor D7/D8/D9 (DIR HIGH = clockwise), laser D10, MPU6050 on A4/A5 (AD0 GND, 0x68, display only).

## 4. Fast counting: 10 ms slices, rolling windows
The loop reads the three pins every ≈ 117 µs (estimate) and counts LOW reads (TSOP output is active-LOW) in **10 ms slices** (≈ 85 reads each). The last 10 slices are kept:
```
c100[i] = sum of the last 10 slices (100 ms)    used for the scan detection (K = 5, spin_until_ir value) and for the printed numbers
c50[i]  = sum of the last 5 slices  (50 ms)     used for the compare / stop (K = 3, same duty ≈ 0.6 %)
```
A new decision is made after every slice. Compared with v0.1/`spin_detect` (a decision at the end of each 50–100 ms window) the detection and stop are noticed up to ~10 ms after the counts cross the threshold instead of up to 100 ms. Each decision uses 50 ms of counts, so one noisy read cannot move the motor. The counts printed are the last 100 ms, so they look like the `spin_detect` numbers (72, 71, …).

## 5. Compare and stop
Every slice (in TURN):
```
c6     = c50[D6]
cOther = largest c50 of the other sensors (D4, D5)             side = which sensor that is

D6 is the greatest   ⇔   c6 ≥ 3   AND   (c6 + 2) · 100 ≥ cOther · CENTRE_PERCENT        (CENTRE_PERCENT = 100)
```
* **D6 is the greatest for 3 slices (30 ms) → STOP** (motor to 0 immediately; the 30 ms are spent at the minimum PWM). The stop is therefore checked 100 times per second and happens as soon as D6 takes the lead.
* Example from your log: `S1: 72  S2: 71  S3: 71` → D6 ≥ the others → stops after 30 ms.
* `CENTRE_PERCENT` > 100 (e.g. 130–150) demands that D6 is *clearly* stronger than the sides (tighter centring when the beams overlap less) but then sensors with the same aim (72/71/71) may never satisfy it, so 100 is the safe default.

## 6. Turning toward the greatest, PID
When D6 is not the greatest, D6 is turned toward the strongest side sensor. The PID sets only the **speed**:
```
mag = (cOther − c6) / (cOther + c6 + 1)           0 … 1, how much stronger the side sensor is (min 0.05)
e   = weight(side) · mag                          setpoint 0, + = clockwise, − = anti-clockwise
I  ← clamp( I + e·dt , ±I_MAX )                   reset when the greatest sensor changes side
D   = 0.8·D + 0.2·(e − e_prev)/dt                 filtered derivative,   dt ≈ 10 ms
u   = Kp·e + Ki·I + Kd·D
direction = toward the greatest side sensor        PWM = clamp(|u|, MIN_TRACK_PWM, MAX_TRACK_PWM)
```
| Term | Role |
|---|---|
| P `KP = 140` | big difference = fast turn (e = 1 → 140), small difference → clamped to the minimum |
| I `KI = 40` | trims a small steady lag; limited to ±40 PWM |
| D `KD = 6` | brakes when the difference is closing quickly (D6 catching up), limiting overshoot |

As D6 catches up, `e` shrinks and the speed falls to the minimum; the moment D6 ties/exceeds the side sensor the stop rule fires. A change of direction first stops the motor for `REVERSE_PAUSE_MS = 30 ms`.

## 7. D6 receive check and give-up
* If D6 is never ≥ 3 during a whole turn, the program prints `WARNING: D6 never received the message - check D6 wiring / aim` – D6 must be on the laser axis and see the beacon.
* If D6 never becomes the greatest within `TRACK_TIMEOUT_MS = 4000`, it goes back to scanning **without stopping and without the laser**, and the signal must clear (200 ms) before the next detection.
* Re-arm after every stop and at power-up: no sensor ≥ K for 20 slices (200 ms).

## 8. Output
Printed every 100 ms:
```
S1(D4): 5  S2(D5): 40  S3(D6): 60  DETECTED | Acc(g) X:-0.68 Y:-0.60 Z:0.51 | Gyro(dps) X:-5.7 Y:2.7 Z:-3.8 | TURN CW pwm130 e:+0.40
```
Same fields as `spin_detect`; the TURN field shows the direction, PWM and PID error `e`. Because the decision loop runs at 100 Hz, the line shows a snapshot of it. A printed line blocks about 6 ms (115200 baud, 64-byte buffer), which only delays the slice that follows.

## 9. Tuning and bench procedure
1. `ir_sensor_test`: confirm D4/D5 are on the sides you configured and D6 is the middle sensor (point the beacon at one side; that sensor must be the strongest). Check D6 gets counts when the beacon is on its axis.
2. **Direction:** with the beacon on the clockwise side the TURN field must read `CW` and the motor must bring D6 toward the beacon; if not, swap `SENSOR_WEIGHT`.
3. **Min PWM:** smallest PWM that reliably turns the assembly → `MIN_TRACK_PWM` (110 is a guess).
4. **Stop:** the motor must stop as soon as `S3(D6)` is the largest number; `>>> STOP (D6 is the greatest)` appears and the laser comes ON for 4.0 s (`--filter time`).
5. **Centring:** if the laser lands off the target, raise `CENTRE_PERCENT` step by step (110, 120, 130); if it then stops less often, go back. Raise `CENTRE_SLICES` for less noise-sensitivity.
6. **PID:** `KP`, `KI`, `KD` only change the turn speed profile; reduce `KP` if it overshoots before the stop fires, raise `MIN_TRACK_PWM` if it stalls.

## 10. Simulation check (not hardware)
A Python model (cosine beam ±45°, count noise, first-order motor response, the same slice/compare/PID code) with 5 random seeds per case:

| Layout (D4 / D5 mounts, D6 = 0°) | `CENTRE_PERCENT` | Stops | Time to stop | Residual pointing error |
|---|---|---|---|---|
| overlapping −30° / +30° | 100 | 100 % | 0.1 – 1.7 s | ≈ 7° – 14° |
| overlapping −30° / +30° | 140 | 100 % | 0.1 – 1.9 s | ≈ −7° – +9° |
| 120° apart | 100 | 100 % | 0.04 – 2.8 s | ≈ 13° – 45° (D6 alone sees the message near its edge) |
| all sensors same aim (72/71/71) | 100 | 100 % | 0.04 – 1.5 s | ≈ 10° – 40° (no spatial information) |
| all sensors same aim | 140 | 60–80 % | up to 5 s | – (the clear-lead rule is never met) |

Reading: the stop now **always happens quickly** (that was the fault), but the aim accuracy is limited by how different the three sensors' views are; the 7–14° with overlapping sensors is the point where D6 takes the lead (halfway between the neighbours) plus braking. Numbers are from the model, not from your hardware.

## 11. Same / different from spin_detect
| | `spin_detect` | `spin_detect_clock` |
|---|---|---|
| Scan, K = 5, 4 s stop with laser ON only, laser OFF before moving, output | same | same |
| Centre sensor | S1 (D4) | **D6** |
| Counting / decision | 100 ms window | 10 ms slices, rolling 50/100 ms, decision at 100 Hz |
| Turn rule | fixed direction per sensor | toward the greatest sensor, PID speed |
| Stop rule | S1 strongest (once per 100 ms) | D6 greatest for 30 ms |
| Turn timeout | stop + laser | scan again, no laser |
