# Spin Until IR – Maths and Process (v0.1 → v0.2)

Goal: the motor spins continuously and stops as soon as any of the IR receivers detects the beacon.
Receiver: Vishay **TSOP4138** – 38 kHz carrier, output **active-LOW**, directivity **±45°**, supply 2.5–5.5 V, range up to 45 m (on-axis).

---

## Version history

| Version | File / env | What it does | Status |
|---|---|---|---|
| v0.1 | `src/ir_sensor_test_main.cpp` / `ir_sensor_test` | Reads S1=D4, S2=D5, S3=D6 for 100 ms and prints LOW-read counts. Motor driver held asleep. Used to measure real counts and pick a threshold. | working |
| v0.2 | `src/spin_until_ir_main.cpp` / `spin_until_ir` | Motor spins; any sensor with count ≥ threshold stops it; resumes after the signal has been clear for 2 windows. | this release – needs bench test |
| v0.3 | `src/directional_ir_main.cpp` / `directional_ir` | 6-sensor ring (60° apart), steer toward the strongest sensor. | written, not yet bench tested |
| v0.4 | – | Tune threshold / window / speed from measured data (section 6). | planned |
| v1.0 | `src/spin_detect_main.cpp` / `spin_detect` | Simple rewrite of `spin_until_ir`: 3 sensors, per-sensor turn direction, 4 s stop with the laser ON, Acc/Gyro output. Maths: [spin_detect.md](spin_detect.md). (`spin_detect_one` keeps the earlier encoder/gyro logic: [spin_detect_one.md](spin_detect_one.md).) | written, needs bench test |

Process per version: **(1)** build the measurement version (v0.1) → **(2)** record counts with the beacon off / on / at ±45° → **(3)** set constants from the maths below → **(4)** add motion (v0.2) → **(5)** re-measure stop angle → **(6)** only then add more sensors.

---

## 1. Sampling window

Each loop iteration reads 3 pins then waits `delayMicroseconds(100)`.

```
t_iter ≈ 3·t_read + t_delay + t_overhead
       ≈ 3·(4.5 µs) + 100 µs + ~3 µs  ≈ 117 µs      (digitalRead ≈ 4–5 µs on a 16 MHz AVR – estimate)
```

Samples per window (`T_w = 100 ms`):

```
N = T_w / t_iter = 100 000 µs / 117 µs ≈ 850 samples
```

## 2. Count and detection rule

Sensor *i* count: `c_i = number of samples where pin i == LOW` (TSOP output is LOW while it sees the 38 kHz burst).

Fraction of the window the signal is present (duty):

```
d_i = c_i / N          →   LOW time ≈ c_i · t_iter
```

Detection rule used in v0.2:

```
detected = (c_1 ≥ K) OR (c_2 ≥ K) OR (c_3 ≥ K),     K = DETECT_THRESHOLD = 5
```

`K = 5` means ≥ 5·117 µs ≈ 0.6 ms of LOW time (d ≈ 0.6 %) inside 100 ms. That rejects single-sample glitches but accepts a very weak or brief signal. Choose K from v0.1 data: `K` should sit above the largest "beacon off" count and well below the smallest "beacon on" count (section 6).

> Note: the TSOP has an AGC and needs a minimum burst length/gap (see datasheet). If the beacon is a steady carrier and counts spike then fall back, the receiver is suppressing it – modulate the transmitter (bursts with gaps) rather than lowering K.

## 3. Reaction time and stop angle

Worst case from signal arriving to motor stopped (window is evaluated at its end):

```
t_react ≤ T_w + t_stop           (T_w = 100 ms, t_stop = mechanical stop time, measure it)
```

Spin speed ω (deg/s) = `6 · rpm`. Angle turned before stopping:

```
Δθ = ω · t_react
```

Example (assumed, not measured): 30 rpm → ω = 180 °/s, t_stop = 50 ms → `Δθ = 180 · 0.15 = 27°`.

The sensor must still be inside its beam when the motor stops, so require `Δθ < β_eff`, where β_eff is the usable beam half-width (≤ 45°, smaller at long range because sensitivity falls off-axis).

## 4. Maximum spin speed (dwell time)

Time the source stays inside one sensor's beam while spinning (beam width β = 2·45° = 90°):

```
t_dwell = β / ω
```

The window is not synchronised to the sweep, so to guarantee one *whole* window inside the beam:

```
t_dwell ≥ 2·T_w   →   ω ≤ β / (2·T_w) = 90 / 0.2 = 450 °/s = 75 rpm
```

Slower than this and K is always reached; faster and the count may only reach `c ≈ N·d·(t_dwell/T_w)` and miss the threshold. The MOTOR_SPEED PWM value (200) must be calibrated to an rpm below this limit.

## 5. Sensor coverage

n sensors, spacing s, beam width β = 90°:

```
if s ≤ β:   coverage = min(360°, (n−1)·s + β)      (no gaps)
if s > β:   coverage = n·β,   gap between sensors = s − β
```

| Layout | Coverage | Worst-case rotation to see the source | Time at 180 °/s |
|---|---|---|---|
| 3 sensors, 60° apart (D4/D5/D6 on a front arc) | 210° | 360 − 210 = 150° | 0.83 s |
| 3 sensors, 120° apart | 270° | s − β = 30° | 0.17 s |
| 4 sensors, 90° apart | 360° | 0° | – |
| 6 sensors, 60° apart (v0.3) | 360°, overlap 30° | 0° | – |

So for a spinning robot, 3 sensors spread at 120° find the beacon within 30° of rotation; 3 sensors clustered on a 60° arc can need up to 150°. This is why v0.3 uses six.

## 6. How to choose the constants (v0.1 measurement procedure)

1. Upload v0.1 and open the monitor.
2. Beacon **off**, record counts for ~30 s → `c_noise_max`.
3. Beacon **on**, on-axis at the working distance, record counts → `c_on_min`.
4. Repeat with the beacon at ±45° off-axis → `c_edge_min`.
5. Set `K` with `c_noise_max < K ≤ c_edge_min` (geometric middle is a good start: `K ≈ √(c_noise_max · c_edge_min)`). If there is no such gap, fix the optics/transmitter first.
6. Check `ω ≤ β / (2·T_w)` and `Δθ < β_eff` for the chosen `MOTOR_SPEED`.

## 7. State logic used in v0.2

```
SPINNING --(any c_i ≥ K)--> STOPPED
STOPPED  --(all c_i < K for 2 consecutive windows)--> SPINNING     (LATCH_ON_DETECT = false)
STOPPED  --(stays stopped until reset)                              (LATCH_ON_DETECT = true)
```

The two-window clear requirement is hysteresis: it stops the motor chattering when the signal is right at the threshold.

---

## 8. v1.0 – Spin detect

The rotating 3-sensor system (environment `spin_detect`) has its own document with the geometry, bearing maths, laser alignment, calibration values and bench procedure: **[spin_detect.md](spin_detect.md)**.

---

## Run commands

Nano V3 (repo root):

```
pio run -e ir_sensor_test -t upload
pio device monitor -e ir_sensor_test

pio run -e spin_until_ir -t upload
pio device monitor -e spin_until_ir

pio run -e spin_detect -t upload
pio device monitor -e spin_detect

pio run -e spin_detect_one -t upload
pio device monitor -e spin_detect_one
```
