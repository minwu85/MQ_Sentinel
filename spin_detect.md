# Spin Detect (v1.0) – simple version, based on spin_until_ir

Environment `spin_detect` · source [src/spin_detect_main.cpp](src/spin_detect_main.cpp) · Nano V3 · **115200 baud**.

```
pio run -e spin_detect -t upload
pio device monitor -e spin_detect
```

> **Status: compiles; not bench tested.** The sensor-to-direction table and all timing values are assumptions taken from the intended layout – check them with the bench procedure in section 9.
> This replaces the earlier encoder/gyro/bearing version (kept as reference in [spin_detect_advanced.md](spin_detect_advanced.md); `spin_detect_one` still uses that logic).

---

## 1. What it does

The motor spins like `spin_until_ir`. Three IR sensors are read in 100 ms windows. When one sees the signal:

```
SCAN (clockwise, laser OFF)
  └─ signal on S1 (laser axis) ─────────────────────────► STOP
  └─ signal on S2 / S3 ─► TURN clockwise / anti-clockwise toward that sensor ─► S1 sees it ─► STOP
STOP: motor off, laser ON for the whole 4 s ─► laser OFF ─► SCAN again
```

* **Laser:** OFF while spinning or turning; ON for the entire stop (4 s). It is the only thing that ever switches D10.
* **Direction depends on the sensor:** S2 → clockwise, S3 → anti-clockwise (table in section 3). If S1 already sees the target the motor just stops.
* **Stop = 4 s**, then the laser goes OFF and the motor spins again. No fault state, no commands, no latch: it always resumes.
* A turn that never reaches S1 ends after 4 s in a stop (laser ON); a turn that loses the signal for 0.5 s returns to scanning.

## 2. Wiring

| Signal | Pin |
|---|---|
| Motor nSLEEP / DIR / PWM | D7 / D8 / D9 (DIR HIGH = clockwise, as `motor_onoff`) |
| S1 / S2 / S3 (TSOP4138, active-LOW, `INPUT_PULLUP`) | D4 / D5 / D6 |
| Laser (HIGH = ON) | D10 |
| MPU6050 VCC / GND | 5 V / common GND with driver and sensors |
| MPU6050 SDA / SCL | A4 / A5 |
| MPU6050 AD0 | GND (address 0x68) |
| MPU6050 INT, XDA, XCL | not connected |

The MPU6050 is for display only; the control does not depend on it, and the program keeps running if it is missing (`IMU:OFF`).

## 3. Sensor → direction, geometry

Intended layout (from the sketch): S1 on the laser axis (0°), S2 at +120° (clockwise of S1), S3 at 240° = −120° (anti-clockwise of S1). Each sensor sees ±45° (TSOP4138), β = 90°.

| Sensor | Pin | Mount | `SENSOR_TURN` | Meaning |
|---|---|---|---|---|
| S1 | D4 | 0° | 0 | on the laser axis → aligned → stop |
| S2 | D5 | +120° | +1 | turn clockwise |
| S3 | D6 | −120° | −1 | turn anti-clockwise |

Shortest-path logic: to bring S1 onto a target that S2 sees, S1 must travel +120° (clockwise); for S3 it is −120° (anti-clockwise). The sign of the mount angle (normalised to ±180°) is the turn direction, so each sensor turns the short way.

Coverage: 3 · 90° = 270° at any instant, three 30° gaps (`120° − 90°`); while scanning, the assembly rotates, so a target is seen after at most 30° of rotation.

Time allowed for the turn: the target can sit anywhere in the sensor's beam, so S1 may have to travel up to `120° + 45° = 165°`. With `TRACK_TIMEOUT_MS = 4 s` this needs a turning speed of at least `165° / 4 s ≈ 41 °/s`; at 60 °/s it takes ≤ 2.75 s.

> **Configure:** if your sensors are wired differently (for example D4 = left, D5 = right, D6 = centre as in `directional_ir`), change `SENSOR_TURN` and `CENTRE_SENSOR` – nothing else needs to change. For that layout: `{ -1, +1, 0 }` and `CENTRE_SENSOR = 2`.

## 4. Detection (same method as spin_until_ir)

Each window counts, per sensor, the reads that are LOW (the TSOP output is active-LOW):
```
loop time ≈ 3 · 4.5 µs (reads) + 100 µs (delay) + ~3 µs ≈ 117 µs          (estimate)
N = WINDOW_MS / loop time = 100 000 µs / 117 µs ≈ 850 samples per window
count_i = number of LOW reads of sensor i          detected = max(count_i) ≥ K,  K = 5  (≈ 0.6 ms LOW, 0.6 % duty)
```
`K = 5` is the `spin_until_ir` value. `CONFIRM_WINDOWS = 1` means one window decides (as `spin_until_ir`); set 2 for more noise immunity at the cost of a further 100 ms.

* **Strongest sensor** `best = argmax count_i` decides the turn direction.
* **Aligned** (stop condition): `count_S1 ≥ K` **and** `count_S1 + ALIGN_MARGIN ≥ count_best` (`ALIGN_MARGIN = 8`, ≈ 11 % of a count of 72). Example from your log, `S1: 72  S2: 71  S3: 71` → S1 is the strongest → aligned → stop immediately. `S2: 60, S1: 30, S3: 5` → turn clockwise; once S1 reaches ≥ 52 → aligned → stop.
* **Re-arm:** after every stop (and at power-up) the signal must be absent for 2 windows (200 ms) before the next detection, so the same target is not detected twice. This matches the "resume only after the signal clears" rule of `spin_until_ir`.

Timing: a window decides at its end, so the signal is recognised within `W` (100 ms) of reaching the sensors, at worst `2·W` if it appears just after a window starts. The assembly keeps moving until the stop: `Δθ = ω · (t_detect + t_stop)`; for ω = 180 °/s (30 rpm), t_detect = 0.15 s, t_stop = 0.05 s → 36°. The turn phase corrects for this by watching S1.

## 5. Turning

* `TRACK_SPEED = 150` (slower than the 200 scan speed, for finer alignment); direction from `SENSOR_TURN[best]`.
* While turning, each window re-checks: aligned → stop; signal lost for `LOST_WINDOWS = 5` windows (0.5 s) → back to scanning; the strongest sensor changes side → reverse; 4 s elapsed → stop.
* A direction change first stops the motor for `REVERSE_PAUSE_MS = 100 ms` to avoid a current spike on the DRV8874.

## 6. Stop and laser

* Order at a stop: **motor off first, then laser ON**. Order at its end: **laser OFF first, then the motor starts**.
* `STOP_MS = 4000`. In the stop state the sampling window is shortened to the time remaining, so the stop ends on time to within about a millisecond; the board's oscillator tolerance adds to that (≈ ±0.2 ms for a crystal, up to ±20 ms for a ceramic resonator).
* Laser rule: D10 is driven LOW first in `setup()`; it is HIGH only inside the STOP state.

| State | Motor | Laser |
|---|---|---|
| power-up / `setup()` | off | OFF |
| SCAN | spinning (clockwise, PWM 200) | OFF |
| TURN | turning toward the sensor (PWM 150) | OFF |
| STOP (4 s) | off | **ON** |

## 7. Output

One line per 100 ms window:
```
S1(D4): 72  S2(D5): 71  S3(D6): 71  DETECTED | Acc(g) X:-0.68 Y:-0.60 Z:0.51 | Gyro(dps) X:-5.7 Y:2.7 Z:-3.8 | SCAN CW
S1(D4): 0  S2(D5): 0  S3(D6): 0  -- | Acc(g) X:0.04 Y:-0.19 Z:0.97 | Gyro(dps) X:-1.5 Y:0.4 Z:-1.4 | SCAN CW
```
| Field | Meaning |
|---|---|
| `S1(D4): n` | LOW reads of that sensor in the last window (name and pin) |
| `DETECTED` / `--` | strongest sensor ≥ K / not |
| `Acc(g) X Y Z` | accelerometer in g (±2 g range, raw/16384) |
| `Gyro(dps) X Y Z` | gyro in degrees per second (±1000 dps range, raw/32.8, bias not removed) |
| last field | `SCAN CW` (`arming` until the signal has cleared), `TURN CW/CCW`, `STOP 3.2s LASER ON` |

Event lines (`>>> S2 detected: turning clockwise toward it <<<`, `>>> STOP (...) - laser ON <<<`, …) are printed when the state changes. `IMU:OFF` replaces the Acc/Gyro part if the MPU6050 does not answer at 0x68 (it is retried every 2 s). The gyro saturates at 1000 °/s (166 rpm). The 14-byte IMU read costs about 0.5 ms per line; at 115200 baud a ~130-character line takes ~11 ms (the serial buffer absorbs the first 64 bytes), all between windows.

## 8. Settings (all constants at the top of the file)

| Constant | Default | Meaning / assumption |
|---|---|---|
| `SENSOR_PINS` | D4, D5, D6 | S1, S2, S3 |
| `SENSOR_TURN`, `CENTRE_SENSOR` | `{0, +1, -1}`, 0 | layout assumption – verify (section 9) |
| `SPIN_SPEED`, `TRACK_SPEED` | 200, 150 | PWM; ω not measured |
| `SCAN_CLOCKWISE` | true | scan direction |
| `WINDOW_MS`, `DETECT_THRESHOLD`, `CONFIRM_WINDOWS` | 100 ms, 5, 1 | from `spin_until_ir` |
| `CLEAR_WINDOWS_TO_REARM` | 2 | 200 ms clear before the next detection |
| `ALIGN_MARGIN` | 8 | S1 within 8 counts of the strongest = aligned |
| `TRACK_TIMEOUT_MS`, `LOST_WINDOWS` | 4000, 5 | give-up / lost-target limits |
| `STOP_MS` | 4000 | stop time with the laser ON |
| `REVERSE_PAUSE_MS` | 100 | pause before reversing |

## 9. Bench procedure

1. Run `ir_sensor_test` first (motor asleep) and note the counts of each sensor with the beacon on and off; `K = 5` must lie between them.
2. Upload `spin_detect`. At power-up `MPU6050 found at 0x68` should appear (else check A4/A5, AD0, power, common GND – scanning still works).
3. **Direction check:** point the beacon at one sensor at a time. S2 should make the motor turn the way that brings S1 toward the beacon (clockwise for the default table); S3 the opposite. If a turn goes the wrong way, flip that entry in `SENSOR_TURN` (and the wiring assumptions in section 3).
4. **Laser check:** the beam must be OFF while spinning/turning and ON for the whole stop. The `STOP x.xs LASER ON` field counts down from 4.0.
5. **Stop time:** `pio device monitor -e spin_detect --filter time`; the time between `>>> STOP (...)` and `>>> STOP finished` must be 4.0 s.
6. Tune `ALIGN_MARGIN` (smaller = stricter centring), `TRACK_SPEED`, `DETECT_THRESHOLD` and `TRACK_TIMEOUT_MS` from what the lines show; check the laser actually points at the beacon when S1 is aligned (the alignment is only as good as the S1 beam shape, ± a fraction of β).
7. IMU: the `Gyro(dps)` column for the spin axis should show the rotation rate while scanning; the other axes stay near 0.

## 10. What changed from the previous spin_detect

Removed: encoder, gyro integration, bearing and shortest-path angle maths, fault states, serial commands, telemetry buffer, `ALIGN_ENABLED`. Added/kept: the `spin_until_ir` counting window, per-sensor turn direction with stop on S1, 4 s stop with the laser ON, the Acc/Gyro columns. Reason: the earlier version depended on several calibrations (counts per revolution, gyro bias, geometry) and stopped on faults; this one needs only the sensors, the motor and the laser.

## 11. Version history

| Version | Environment | Step |
|---|---|---|
| v0.1 | `ir_sensor_test` | measure counts per sensor, motor asleep |
| v0.2 | `spin_until_ir` | spin, stop on detection, resume when clear (proven base) |
| v0.3 | `directional_ir` | 6-sensor ring, steer toward the strongest sensor |
| v0.5–v0.9 | `spin_detect` (earlier) | encoder + gyro + bearing + alignment – archived in [spin_detect_advanced.md](spin_detect_advanced.md) |
| **v1.0** | `spin_detect` (this) | simple rewrite of `spin_until_ir`: per-sensor turn direction, 4 s stop with the laser ON, Acc/Gyro output |
