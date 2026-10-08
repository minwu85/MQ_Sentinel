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
| v0.5 | `src/rotating_detect_main.cpp` / `rotating_detect` | Rotating assembly, 3 sensors 120° apart, debounced confirmed trigger, immediate stop, encoder position, laser mark, timeout/fault states (section 8). | written, needs bench test |

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

## 8. v0.5 – Rotating detection system

### 8.1 Geometry
Sensors at mount angles `φ_i = i · 360°/3 = 0°, 120°, 240°` (S1=D4, S2=D5, S3=D6), beam width β = 90°.
Instantaneous coverage = 3 · 90° = **270°**; three 30° gaps centred on 60°, 180°, 300° (assembly frame). The assembly rotates, so a source in a gap enters a beam after at most `s − β = 120° − 90° = 30°` of rotation. Full 360° coverage therefore comes from **rotation**, not from the static beams.

### 8.2 Window, debounce, confirm
```
sample period = 100 µs  (timed with micros(), not delayMicroseconds)
WINDOW_MS     = 20      →  N ≈ 20 000 / 100 = 200 samples per window
DETECT_K      = 20      →  duty ≥ 20/200 = 10 % of the window ("clearly in the beam")
CONFIRM       = 3       →  same single sensor, 3 consecutive windows
```
Scale K from v0.1 data: `K_v0.5 = K_clear · (200 / N_v0.1)` where `N_v0.1 ≈ 850` per 100 ms window.

False-trigger estimate: if one sensor wrongly passes a single window with probability `p`, a false confirm needs 3 in a row on the same sensor:
```
P_false ≈ 3 · p³ per window-triple      p = 1 %  →  3·10⁻⁶  →  at 50 windows/s ≈ 1.5·10⁻⁴ /s  (≈ one per 1.8 h)
```
(assumes independent windows – real noise is bursty, so measure `p` with v0.1 and treat this as a best case.)

Validation rules (all must hold, otherwise nothing is accepted):
1. **Armed**: all sensors clear for 5 windows (100 ms) after every (re)start, so a target that is still in the beam is not detected twice.
2. **Exactly one** sensor above K – the beams do not overlap at 120° spacing, so two at once means noise/flooding. 10 consecutive such windows → `MULTI_SENSOR` fault.
3. **Stuck sensor**: counts ≥ 95 % of samples for `STUCK_MS = 3 s` → `SENSOR_STUCK`. Needs `STUCK_MS > β/ω`, i.e. `ω > 90°/3 s = 30 °/s (5 rpm)` or a legitimate beam crossing looks stuck.

### 8.3 Stop latency and overshoot
```
t_react ≤ (CONFIRM + 1) · WINDOW_MS + t_stop = 80 ms + t_stop       (+1 window: confirm starts mid-window)
Δθ = ω · t_react                                                    e.g. 30 rpm = 180 °/s, t_stop 50 ms → 180·0.13 ≈ 23°
```
The sensor must stay inside its beam long enough to be confirmed:
```
t_dwell = β_eff / ω ≥ (CONFIRM + 1) · WINDOW_MS      →   ω ≤ β_eff / 80 ms
β_eff = 90°  →  ω ≤ 1125 °/s (187 rpm)         β_eff = 60° (long range, off-axis) → 750 °/s (125 rpm)
```
The motor is stopped with PWM = 0 before any serial printing. In PH/EN mode the DRV8874 should brake at EN = 0 (check on the bench); `t_stop` is measured, not assumed.

### 8.4 Position tracking
Encoder A (D2, rising edge) with B (D3) as direction – same scheme as `connection_test`.
```
θ = sign · counts · 360 / COUNTS_PER_REV          resolution = 360 / COUNTS_PER_REV
COUNTS_PER_REV = encoder pulses per motor rev × gear ratio   (or: turn the assembly one full turn by hand, send 'p', read counts)
```
`sign` (±1) is learned automatically from the first 500 ms of rotation so that rotating forward = increasing angle.
Reported bearing in the assembly's zero frame: `bearing = θ_stop + φ_i`, uncertain by ±β/2 = ±45° because the sensor sees the source anywhere across its beam. (Planned improvement: record the angle where the source enters and leaves a beam and use the midpoint.)

### 8.5 Laser mark
The laser axis sits at `LASER_MOUNT_DEG` (0° = S1 axis). To point it at the detected sensor's axis the assembly turns
```
offset = wrap180(φ_i − φ_laser)  →  S1: 0°,  S2: +120°,  S3: −120°      (≤ 120° · shortest way)
```
at `ALIGN_SPEED`, stopping within `ALIGN_TOL_DEG = 3°`. Needs `COUNTS_PER_REV` set; otherwise alignment is skipped and the laser fires where the motor stopped. Align must finish inside 4 s → `ω_align ≥ 120°/4 s = 30 °/s`.
Laser safety: off at power-up, off while rotating, 300 ms pulse, hard cap `LASER_MAX_MS = 1 s`, forced off in every fault. Duty cycle = `0.3 s / cycle time` (≈ 3 % for a 10 s cycle).

### 8.6 Timeouts and faults
| Fault | Condition | Result |
|---|---|---|
| `TIMEOUT` | no confirmed detection for 30 s, or > 5 revolutions (when calibrated) | motor off, laser off, driver asleep |
| `ENCODER_STALL` | < 3 encoder counts in 500 ms while driven (after 600 ms spin-up) | same |
| `SENSOR_STUCK` | one sensor ≥ 95 % LOW for 3 s | same |
| `MULTI_SENSOR` | ≥ 2 sensors above K for 10 consecutive windows | same |
| `ALIGN_TIMEOUT` | laser alignment not reached in 4 s, or moving the wrong way | same |

Leave FAULT with `r` over serial. A sensor that is unplugged reads HIGH (pull-up) and cannot be told apart from "no beacon"; it shows up as `TIMEOUT`.

### 8.7 State machine
```
ROTATING --(armed, 1 sensor ≥K × 3 windows)--> STOP --> ALIGN (optional) --> INDICATE (laser 300 ms) --> PAUSE 1 s --> ROTATING
ROTATING / ALIGN --(any fault)--> FAULT --('r')--> ROTATING
```

### 8.8 Bench test order
1. Run v0.1 (`ir_sensor_test`) – record noise and beacon counts, then set `DETECT_K`.
2. Upload v0.5 with the **motor disconnected from the assembly**; check `p` shows counts changing when the shaft is turned by hand; set `COUNTS_PER_REV`.
3. Motor on, beacon off: confirm `TIMEOUT` fires and `r` restarts.
4. Beacon on S1, then S2, then S3: confirm `DETECT`, stop, laser pulse, resume.
5. Cover/unplug the encoder: confirm `ENCODER_STALL`.

---

## Run commands

Nano V3 (repo root):

```
pio run -e ir_sensor_test -t upload
pio device monitor -e ir_sensor_test

pio run -e spin_until_ir -t upload
pio device monitor -e spin_until_ir

pio run -e rotating_detect -t upload
pio device monitor -e rotating_detect
```
