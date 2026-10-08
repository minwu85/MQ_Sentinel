# Spin Detect One (v0.2) – single detection channel, single laser

Environment `spin_detect_one` · source [src/spin_detect_one_main.cpp](src/spin_detect_one_main.cpp) · Nano V3.
Simplified, fully automatic sibling of `spin_detect` ([spin_detect_advanced.md](spin_detect_advanced.md)): one TSOP4138 channel and one laser, **no serial input** (no `g`, `r`, space, `l`). Shared maths (gyro, encoder, cross-checks, hold timing) is identical and is summarised below; the three-sensor geometry is removed.

```
pio run -e spin_detect_one -t upload
pio device monitor -e spin_detect_one        (output only – telemetry)
```

> **Status: compiles; not bench tested.** All geometry and calibration values are configurable placeholders. `ALIGN_ENABLED = true` by default (the assembly rotates onto the target before the laser turns on); set it to `false` to stop in place while `COUNTS_PER_REV` and the laser/sensor geometry are unverified (section 11).

---

## 1. Automatic sequence

```
power-up → init (laser OFF, motor driver, encoder, MPU6050 verify + bias) → auto-start
 → continuous spin (laser OFF) → strong message → confirm (window count + 3-window debounce + armed)
 → motor stops immediately → record encoder, gyro, bearing
 → [ALIGN_ENABLED, needs an angle source] rotate the shortest way onto the target (laser OFF)
 → laser ON at the confirmed stopped position → hold 5 s → laser OFF → resume spin → repeat
```

No operator action is needed at any point. A fault stops the motor, turns the laser OFF and restarts automatically (section 8).

## 2. Wiring

| Signal | Pin | Notes |
|---|---|---|
| Motor nSLEEP / DIR / PWM | D7 / D8 / D9 | unchanged from `motor_onoff` / `spin_until_ir` |
| Detection sensor (TSOP4138, active-LOW) | D6 | `INPUT_PULLUP`; the `IR_CENTRE` position used by `reactive_ir` |
| Encoder A / B | D2 / D3 | A on interrupt 0 (rising edge), B = direction |
| Laser | D10 | `HIGH` = ON; eye-safe module |
| MPU6050 VCC / GND | 5 V / **common GND** with motor driver, sensor, encoder | |
| MPU6050 SDA / SCL | **A4 / A5** | hardware I²C (`Wire`) |
| MPU6050 AD0 | GND | address **0x68** |
| MPU6050 INT, XDA, XCL | not connected | |

## 3. Geometry (single path)

* One sensor with mount angle `φ = SENSOR_MOUNT_DEG` and one laser with axis `ψ = LASER_MOUNT_DEG`, both radial. Default `φ = ψ = 0°` (laser co-axial with the sensor).
* Field of view β = 90° (TSOP4138 ±45°), assumed – measure it.
* Instantaneous coverage = β = **90°**. The assembly spins, so the target is inside the beam for β/ω of every revolution (30 rpm = 180 °/s → 0.5 s of each 2 s).
* Worst-case rotation before the first detection = `360° − β = 270°` (1.5 s at 180 °/s, 0.75 s on average). The three-sensor version needs at most 30° (`s − β`).
* Parallax (only if `SENSOR_RADIUS_MM` and `TARGET_DISTANCE_MM` are set): `α_C = atan2( D·sin α , R + D·cos α )` – R = 50 mm, α = 45°, D = 1000 mm → 43.0° (2.0° error).

## 4. Detection and debounce (spin_until_ir approach)
Count LOW reads per window (TSOP output is active-LOW); `count ≥ K` = signal.

| Parameter | `spin_until_ir` v0.2 | `spin_detect_one` |
|---|---|---|
| sample period | ~117 µs loop | 100 µs (`micros()`) |
| window | 100 ms (N ≈ 850) | 20 ms (N ≈ 200) |
| K | 5 | 5 |
| confirm | 1 window | 3 consecutive windows |
| clear before re-arm | 2 windows = 200 ms | 10 windows = 200 ms |

Required for a detection: **armed** (sensor clear 200 ms and the angle-source sign known, so a target still in the beam cannot re-trigger) and **3 consecutive windows ≥ K**. A sensor ≥ 95 % LOW for 3 s → `SENSOR_STUCK`. There is no multi-sensor rule here.

### Detection-to-stop timing
```
t_react ≤ (CONFIRM + 1) · WINDOW_MS + t_stop = 80 ms + t_stop            motor stops before anything is printed
Δθ = ω · t_react                                                          180 °/s, t_stop 50 ms → 23°
t_laser_on = t_react + t_print [+ t_align]                                t_print ≈ 0.1–0.2 s (DETECT lines at 9600 baud)
dwell: β_eff / ω ≥ 80 ms  →  ω ≤ β_eff / 0.08 s  (1125 °/s for 90°)
```
The 5 s hold starts at `enterHold()`, the instant the laser turns ON, so printing never shortens it.

## 5. Encoder, gyro and pose
Encoder: `θ_enc = rotateSign · counts · 360 / COUNTS_PER_REV` (resolution `360/COUNTS_PER_REV` deg/count; `COUNTS_PER_REV = 0` = uncalibrated).
MPU6050 gyro: enabled only after ACK at 0x68, `WHO_AM_I = 0x68`, `GYRO_CONFIG` read-back and a bias calibration (spread ≤ 4 dps over 1 s). If the assembly is still moving (e.g. coasting after a reset) the calibration waits and retries every second, up to 10 times; if the gyro still failed at boot it tries once more during the next hold.
```
ω = raw / LSB − bias                 LSB = 32.8 per deg/s at the default ±1000 dps range
θ_gyro += gyroSign · ω · Δt          Δt ≈ 10 ms (100 Hz), |ω| < 0.3 dps ignored
```
The signs (`rotateSign`, `gyroSign`) are learned during the first scan so angle increases with `DIR = HIGH`. Bias is re-measured during each hold when the data are clean.

| Purpose | Rule |
|---|---|
| angle source | encoder if `COUNTS_PER_REV > 0`, otherwise the verified gyro, otherwise none |
| rotation check (500 ms) | encoder ≥ 3 counts or gyro ≥ 3°; neither → `NO_MOTION` |
| cross-check | `|Δθ_enc − Δθ_gyro| ≤ max(8°, 15 %)`; 3 bad checks → `POS_MISMATCH`; encoder silent while gyro moves → `POS_MISMATCH` |
| alignment check | encoder and gyro must agree on the move before the laser may turn on |

## 6. Target angle
`θ_first` = pose at the start of the first window that reached K. With scan direction `d = SCAN_DIR`:
```
B = θ_first + φ + d · h               h = EDGE_HALF_DEG (placeholder 45°, or the parallax-corrected α_C)
angle_error = wrap180( B − (θ_stop + ψ) )     shortest path, ±180°, positive = DIR HIGH
```
Co-axial case (`φ = ψ = 0`, `d = +1`): `angle_error = h − (θ_stop − θ_first)` – e.g. 45° − 23° = **+22°**: the laser at the stop position is 22° short of the target. Other examples: B = 10°, θ_stop = 350° → +20° (not −340°); B = 300°, θ_stop = 100° → −160°.
With `ALIGN_ENABLED = true` and `|angle_error| > 5°` the assembly turns at `ALIGN_SPEED`, braking when `remaining ≤ ALIGN_LEAD_DEG` (3°), then after 150 ms accepts if `|remaining| ≤ 5°` and the encoder/gyro agree; the laser is **OFF** during that turn. Faults: wrong way, 8 s timeout, final error > 5° or disagreement → `ALIGN_FAIL`.

## 7. Laser state machine

| State | Motor | Laser | Leaves when |
|---|---|---|---|
| `setup()` / boot | off | **OFF** (explicit, first statement) | init done |
| SCANNING | spinning | **OFF** | confirmed detection |
| ALIGN (only if `ALIGN_ENABLED`) | turning | **OFF** | aligned and verified |
| HOLD (5 s) | stopped (brake) | **ON** – marks the target | `millis() − holdStart ≥ 5000` |
| FAULT | off, driver asleep | **OFF** | automatic retry |

`applyLaser()` is the only code that drives D10 and allows ON only in HOLD. `enterRotating()` sets the state (laser LOW) before the motor starts, so the laser is OFF before scanning resumes. Hold timing: `millis()` ticks 1 ms, the loop checks it every pass, so the hold is 5000 ms plus up to ~1 ms plus the oscillator error (≈ ±0.25 ms for a crystal, up to ±25 ms for a ceramic resonator). The hold is passive braking; the drift is printed at the end (`hold drift = x deg`).

## 8. Faults and automatic restart

| Fault | Condition |
|---|---|
| `NO_MOTION` | neither encoder nor gyro sees rotation in 500 ms (after 600 ms spin-up) |
| `SENSOR_STUCK` | sensor ≥ 95 % LOW for 3 s |
| `ALIGN_FAIL` | alignment not reached/verified |
| `POS_MISMATCH` | encoder vs gyro disagree, or encoder silent while gyro moves |
| `TIMEOUT` | only if `SCAN_TIMEOUT_MS > 0` (default 0 = scan indefinitely) |

On a fault: motor stopped, driver asleep, laser OFF. After `FAULT_RETRY_MS` (5 s) it restarts automatically, up to `MAX_AUTO_RETRIES` (3) times in a row; a completed hold resets the counter. When the retries are used up it stays safe (motor and laser off) until power is cycled. A persistent `NO_MOTION` therefore causes 3 short drive attempts of ~1 s each.

## 9. Telemetry (output only)
One line every `TELEMETRY_MS` (500 ms), built in a buffer and fed to the UART as space frees up so it never blocks the 100 µs sampling loop:
```
Acc(g) X:0.04 Y:-0.19 Z:0.97 | Gyro(dps) X:-1.5 Y:0.4 Z:-1.4 | T:26.6C | Enc:1234cnt 45.2deg | GyroAng:44.8deg | Sig:7/200lo conf2/3 | Det:3 Brg:155.0deg LaserAx:0.0deg Err:+141.0deg | Mot:CW pwm200 | Laser:OFF | SCAN armed
```
| Field | Meaning / unit |
|---|---|
| `Acc(g) X Y Z` | accelerometer, g (±2 g) |
| `Gyro(dps) X Y Z` | raw gyro, deg/s (bias not removed) |
| `T` | temperature, °C = raw/340 + 36.53 |
| `Enc` | counts and angle, deg (`n/a` until `COUNTS_PER_REV` set), 0–360 |
| `GyroAng` | integrated gyro angle, deg, 0–360 |
| `Sig` | LOW reads / samples (`lo`) in the last 20 ms window; `conf n/3` = consecutive confirming windows |
| `Det` | number of confirmed detections since boot |
| `Brg` | last calculated target bearing, deg |
| `LaserAx` | laser-axis angle `pose + ψ`, deg |
| `Err` | live `angle_error`, signed deg, only during ALIGN/HOLD, else `--` |
| `Mot` | `OFF` or `CW`/`CCW` + PWM |
| `Laser` | `ON`/`OFF` |
| state | `SCAN armed/arming` · `ALIGN rem:±x deg` · `HOLD x.x s left` · `FAULT <name>` |

`IMU:OFF` replaces the first three fields if the MPU6050 was not verified or dropped out. The burst IMU read costs ≈ 0.5 ms per interval; at 9600 baud a ~220-character line uses ~46 % of the line capacity.

## 10. Assumptions and calibration values

| Constant | Meaning | Default | Assumption / status |
|---|---|---|---|
| `SENSOR_PIN` | detection input | D6 | same position as `reactive_ir` – confirm wiring |
| `SENSOR_MOUNT_DEG` | sensor axis angle | 0° | layout – verify |
| `LASER_MOUNT_DEG` | laser axis angle | 0° (co-axial) | layout – verify; lateral offset assumed 0 |
| `EDGE_HALF_DEG` | half-angle where count reaches K | 45° | **placeholder** (datasheet directivity) – measure |
| `COUNTS_PER_REV` | encoder counts per turn | 0 | **measure** |
| `SENSOR_RADIUS_MM`, `TARGET_DISTANCE_MM` | parallax | 0 (ignored) | optional |
| `SCAN_DIR` | scan direction | +1 (DIR HIGH) | layout |
| `DETECT_K`, window, confirm, re-arm | threshold / debounce | 5, 20 ms, 3, 200 ms | from `spin_until_ir`; tune |
| `SPIN_SPEED`, `ALIGN_SPEED` | PWM | 200, 150 | measure ω |
| `ALIGN_LEAD_DEG`, `ALIGN_ACCEPT_DEG` | brake lead, accept band | 3°, 5° | tune with `t_stop` |
| `GYRO_AXIS`, `GYRO_RANGE_SEL` | spin axis, range | Z, ±1000 dps | verify |
| `HOLD_MS` | hold | 5000 | check with timestamps |
| `ALIGN_ENABLED` | rotate to target | **true** | `false` = stop in place while the geometry is unverified |
| `MAX_AUTO_RETRIES`, `FAULT_RETRY_MS` | auto-restart | 3, 5000 | design choice |

Other assumptions: the beacon produces a 38 kHz signal the TSOP can pass (a steady carrier may be suppressed by its AGC); `HIGH` on D10 switches the laser ON; the motor brakes with `PWM = 0` in PH/EN mode (verify); the MPU6050 is mounted flat with its Z axis parallel to the spin axis.

## 11. Bench procedure (alignment stays disabled until step 8)

1. **Power-up check.** The banner must show `MPU6050 verified at 0x68 - gyro logic ENABLED` (still assembly). If not, check A4/A5, AD0, 5 V and the common ground.
2. **Laser off while scanning.** Watch `Laser:OFF` in the telemetry and the beam itself during scanning; it must light only after a detection.
3. **Gyro axis and sign.** Turn the assembly by hand about the spin axis: `Gyro(dps)` on the `GYRO_AXIS` column must be large, the others small; after the first scan the log shows `gyro sign learned`.
4. **`COUNTS_PER_REV`.** Turn one full revolution by hand against a mark, read `Enc` counts; repeat 3× and average; set the constant; then `Enc` degrees and `GyroAng` should track each other within the cross-check tolerance.
5. **Spin speed and stop time.** Time a revolution (ω). After a detection, `stop − first_seen` = `ω·(≈80 ms + t_stop)` → `t_stop`; set `ALIGN_LEAD_DEG ≈ ω_align·t_stop`.
6. **Beam width and `EDGE_HALF_DEG`.** Place a beacon at a known bearing `B_known`; over ≥ 10 passes read `first_seen` in the `DETECT` line and take `h = d·wrap180(B_known − θ_first − φ)`; average.
7. **Threshold.** Beacon off → noise max; beacon on at the beam edge → min; keep `K` between them, checked with telemetry running.
8. **Bearing, then alignment.** With `ALIGN_ENABLED = false` compare `Brg` with `B_known` (accept ≤ 5°). Then set `ALIGN_ENABLED = true` (default) with an LED on D10 instead of the laser; confirm the shortest direction, `aligned, residual error`, completion inside 8 s, and the LED lit only in HOLD. Connect the laser and confirm the beam hits the target.
9. **Hold timing.** `pio device monitor -e spin_detect_one --filter time`: `>>> HOLD` to the next `>>> SCANNING` must be 5.0 s.
10. **Fault recovery.** Stall the motor briefly: expect `NO_MOTION`, laser OFF, an automatic restart after 5 s, and a safe stop after 3 failed retries.

## 12. Differences: spin_detect vs spin_detect_one

| | `spin_detect` | `spin_detect_one` |
|---|---|---|
| Detection channels | 3 sensors, D4/D5/D6 at 0°/120°/240° | 1 sensor, D6 |
| Laser | D10, ON only in HOLD | D10, ON only in HOLD (same) |
| Instantaneous coverage / worst-case first detection | 270° / 30° of rotation | 90° / 270° of rotation |
| Sensor identity, several sensors at once | yes, strongest sensor wins | none (single channel) |
| Geometry constants | `SENSOR_MOUNT_DEG[3]`, `LASER_MOUNT_DEG` | `SENSOR_MOUNT_DEG`, `LASER_MOUNT_DEG` |
| Serial input | `g`, space, `r`, `z`, `p`, `l` | none – fully automatic |
| After a fault | waits for `g` | automatic restart (3 tries, 5 s apart), then safe |
| Scan direction | reversible with `r` | fixed `SCAN_DIR` |
| Telemetry | `Sig:S<n> …`, `Det:S<n>` | `Sig:…`, `Det:<count>` |
| Encoder, gyro, cross-checks, shortest-path maths, hold, `ALIGN_ENABLED` default | identical | identical |

## 13. Coding process
`spin_detect_one` was derived from `spin_detect` (v0.8): the sensor array, multi-sensor logic, sensor naming and the serial command handler were removed; the single-channel window count, armed/confirm logic, bearing, shortest-path alignment, encoder/gyro tracking, telemetry and hold were kept; an automatic fault-retry state was added because no command can clear a fault. Workflow per change: edit → `pio run -e spin_detect_one` → bench-test → record the measured values in section 10.
