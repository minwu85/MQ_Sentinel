# MQ Sentinel — Engineering Mathematics

> This file collects the engineering mathematics used for MQ Sentinel in a GitHub-friendly format.  
> Numerical results are calculated from values already recorded in the project documentation and current control code.  
> **Important:** the recorded mass total is a current known-component total, not the final complete satellite mass. The project document itself notes that the final mass will be higher.

---

## 1. Known Numerical Inputs

| Quantity | Value |
|---|---:|
| Current known component mass | **1.0849 kg** |
| Gravity, `g` | **9.81 m/s²** |
| FIT0186 gearbox ratio | **43.8:1** |
| FIT0186 maximum output speed | **251 rpm** |
| FIT0186 encoder resolution | **700 counts/rev** |
| FIT0186 stall torque | **1.77 N·m** |
| FIT0186 approximate stall current | **7 A** |
| Approx. motor resistance used in braking analysis | **1.71 Ω** |
| Approx. back-EMF constant used in braking analysis | **0.456 V·s/rad** |
| Supply voltage used in braking analysis | **12 V** |
| Flywheel diameter | **100 mm** |
| Flywheel radius | **0.050 m** |
| Flywheel thickness | **10 mm = 0.010 m** |
| Mild-steel density | **7850 kg/m³** |
| Recorded flywheel mass | **0.616 kg** |
| Current motor-control PWM scan value | **200 / 255** |
| Current tracking PWM range | **110–200 / 255** |
| Controlled deceleration time | **1000 ms = 1.0 s** |
| Control slice time | **10 ms** |
| Rolling scan window | **100 ms** |
| Rolling tracking window | **50 ms** |
| Scan detection threshold | **5 LOW samples / 100 ms** |
| Tracking threshold | **3 LOW samples / 50 ms** |
| Laser hold in current code | **4000 ms = 4.0 s** |
| Minimum project laser-on requirement | **2.0 s** |

---

# Mechanical Mathematics

## 2. Current Mass Budget

The currently recorded component masses are:

| Component | Mass (kg) | Share of current known mass |
|---|---:|---:|
| Arduino Nano V3.0 | 0.0070 | 0.65% |
| 6× Eneloop rechargeable AA batteries | 0.1620 | 14.93% |
| 6×AA battery holder | 0.0140 | 1.29% |
| FIT0186 DC gearmotor + encoder | 0.2050 | 18.90% |
| 100 mm × 10 mm mild-steel flywheel | 0.6160 | 56.78% |
| Shaft mounting hub/coupler | 0.0070 | 0.65% |
| DRV8874 carrier | 0.0009 | 0.08% |
| USB charger | 0.0730 | 6.73% |

### Total mass

```math
M = \sum m_i
```

```math
M = 0.007 + 0.162 + 0.014 + 0.205 + 0.616 + 0.007 + 0.0009 + 0.073
```

```math
\boxed{M = 1.0849\text{ kg}}
```

The flywheel is currently the dominant known mass:

```math
\frac{0.616}{1.0849}\times100
= \boxed{56.78\%}
```

---

## 3. Static Weight / Load Bearing

The gravitational load from the currently known mass is:

```math
W = Mg
```

```math
W = 1.0849\times9.81
```

```math
\boxed{W = 10.643\text{ N}}
```

Equivalent kilogram-force:

```math
W_{kgf} = \frac{10.643}{9.81}
= \boxed{1.0849\text{ kgf}}
```

This **10.643 N is a lower bound**, because PVC, laser, sensors, final printed structure, wiring and other final hardware are not all included in the recorded mass total.

### Equal vertical supports

For `n` perfectly vertical supports sharing the load equally:

```math
T_{each} = \frac{W}{n}
```

Two supports:

```math
T_2 = \frac{10.643}{2}
= \boxed{5.321\text{ N per support}}
```

Three supports:

```math
T_3 = \frac{10.643}{3}
= \boxed{3.548\text{ N per support}}
```

Four supports:

```math
T_4 = \frac{10.643}{4}
= \boxed{2.661\text{ N per support}}
```

### Two-line suspension at an angle

If two equal suspension lines are each at angle `θ` from vertical:

```math
2T\cos\theta = W
```

```math
T = \frac{W}{2\cos\theta}
```

Using the current known load:

| Angle from vertical | Calculation | Tension in each line |
|---:|---|---:|
| 0° | `10.643 / (2 cos 0°)` | **5.321 N** |
| 30° | `10.643 / (2 cos 30°)` | **6.145 N** |
| 45° | `10.643 / (2 cos 45°)` | **7.526 N** |
| 60° | `10.643 / (2 cos 60°)` | **10.643 N** |

### Factor-of-safety design loads

If a factor of safety is applied directly to the current static load:

For `FoS = 2`:

```math
F_{design,2} = 2(10.643)
= \boxed{21.286\text{ N}}
```

For `FoS = 3`:

```math
F_{design,3} = 3(10.643)
= \boxed{31.929\text{ N}}
```

These are design-check values, not measured dynamic loads.

---

## 4. Centre of Mass / Centre of Gravity

For discrete components:

```math
x_{CG} = \frac{\sum m_i x_i}{\sum m_i}
```

```math
y_{CG} = \frac{\sum m_i y_i}{\sum m_i}
```

```math
z_{CG} = \frac{\sum m_i z_i}{\sum m_i}
```

Using the current known mass:

```math
x_{CG} =
\frac{\sum m_i x_i}{1.0849}
```

```math
y_{CG} =
\frac{\sum m_i y_i}{1.0849}
```

```math
z_{CG} =
\frac{\sum m_i z_i}{1.0849}
```

### Ideal centred-design result

If every component centre is placed exactly on the intended rotation axis, then:

```math
x_i = 0,\quad y_i = 0
```

Therefore:

```math
x_{CG} = \frac{1.0849(0)}{1.0849}=\boxed{0\text{ mm}}
```

```math
y_{CG} = \frac{1.0849(0)}{1.0849}=\boxed{0\text{ mm}}
```

The radial CG error is:

```math
r_{CG} = \sqrt{x_{CG}^2+y_{CG}^2}
```

Ideal case:

```math
r_{CG} = \sqrt{0^2+0^2}
= \boxed{0\text{ mm}}
```

### Actual CG still requires final CAD/measured coordinates

The current project files provide component masses but **do not provide final `x, y, z` coordinates for each component**, so a truthful numerical value for the real final CG cannot yet be calculated.

Use this table once the final CAD coordinates are measured:

| Component | Mass (kg) | x (m) | y (m) | z (m) | `m·x` | `m·y` | `m·z` |
|---|---:|---:|---:|---:|---:|---:|---:|
| Arduino Nano | 0.0070 | TODO | TODO | TODO | TODO | TODO | TODO |
| 6× AA batteries | 0.1620 | TODO | TODO | TODO | TODO | TODO | TODO |
| Battery holder | 0.0140 | TODO | TODO | TODO | TODO | TODO | TODO |
| FIT0186 motor | 0.2050 | TODO | TODO | TODO | TODO | TODO | TODO |
| Flywheel | 0.6160 | TODO | TODO | TODO | TODO | TODO | TODO |
| Hub | 0.0070 | TODO | TODO | TODO | TODO | TODO | TODO |
| DRV8874 | 0.0009 | TODO | TODO | TODO | TODO | TODO | TODO |
| USB charger | 0.0730 | TODO | TODO | TODO | TODO | TODO | TODO |

---

## 5. Imbalance Force from an Off-Centre CG

Rotational imbalance force:

```math
F_u = M r_{CG} \omega^2
```

A documented design-analysis speed used elsewhere in the project is `30 rpm = 180°/s`.

Convert to radians per second:

```math
\omega
=
30\frac{2\pi}{60}
=
\boxed{3.1416\text{ rad/s}}
```

Using `M = 1.0849 kg`:

| CG offset | Calculation | Imbalance force |
|---:|---|---:|
| 1 mm | `1.0849 × 0.001 × 3.1416²` | **0.0107 N** |
| 5 mm | `1.0849 × 0.005 × 3.1416²` | **0.0535 N** |
| 10 mm | `1.0849 × 0.010 × 3.1416²` | **0.1071 N** |

The equation shows why balancing matters:

```math
F_u \propto r_{CG}
```

and

```math
F_u \propto \omega^2
```

Doubling rotation speed produces four times the imbalance force.

---

# Reaction Wheel Mathematics

## 6. Flywheel Volume and Mass

The baseline flywheel is a solid mild-steel disk.

Radius:

```math
r = \frac{100\text{ mm}}{2}
= 50\text{ mm}
= 0.050\text{ m}
```

Thickness:

```math
h=10\text{ mm}=0.010\text{ m}
```

Disk volume:

```math
V=\pi r^2h
```

```math
V=\pi(0.050)^2(0.010)
```

```math
\boxed{V=0.00007854\text{ m}^3}
```

Mass:

```math
m=\rho V
```

```math
m=7850(0.00007854)
```

```math
\boxed{m=0.6165\text{ kg}}
```

The project mass table rounds this to:

```math
\boxed{m_w\approx0.616\text{ kg}}
```

---

## 7. Flywheel Moment of Inertia

For a solid disk:

```math
I_w=\frac12mr^2
```

Using the project rounded mass:

```math
I_w
=
\frac12(0.616)(0.050)^2
```

```math
\boxed{I_w=0.000770\text{ kg·m}^2}
```

---

## 8. Motor / Flywheel Angular Speed

Motor output speed:

```math
N=251\text{ rpm}
```

Convert to rad/s:

```math
\omega
=
N\frac{2\pi}{60}
```

```math
\omega
=
251\frac{2\pi}{60}
```

```math
\boxed{\omega=26.2847\text{ rad/s}}
```

Convert to degrees per second:

```math
\omega_{deg/s}
=
251\frac{360}{60}
=
\boxed{1506\text{ °/s}}
```

---

## 9. Flywheel Angular Momentum

```math
H=I\omega
```

```math
H
=
(0.000770)(26.2847)
```

```math
\boxed{H=0.02024\text{ N·m·s}}
```

For an ideal isolated reaction-wheel system:

```math
I_s\omega_s + I_w\omega_w = 0
```

Therefore:

```math
\boxed{\omega_s=-\frac{I_w}{I_s}\omega_w}
```

A final numerical chassis angular velocity cannot be calculated until the complete chassis moment of inertia `I_s` is measured/calculated from the finished geometry.

---

## 10. Flywheel Rotational Kinetic Energy

```math
E_k=\frac12I\omega^2
```

```math
E_k
=
\frac12(0.000770)(26.2847)^2
```

```math
\boxed{E_k=0.2660\text{ J}}
```

If speed is halved:

```math
E_{half}
=
\frac12 I\left(\frac{\omega}{2}\right)^2
=
\frac14E_k
```

```math
E_{half}
=
\frac{0.2660}{4}
=
\boxed{0.0665\text{ J}}
```

So halving speed reduces rotational energy by:

```math
0.2660-0.0665
=
\boxed{0.1995\text{ J}}
```

or **75%**.

---

## 11. Flywheel Rim Speed and Centripetal Acceleration

Rim speed:

```math
v=\omega r
```

```math
v=(26.2847)(0.050)
```

```math
\boxed{v=1.3142\text{ m/s}}
```

Rim centripetal acceleration:

```math
a_c=\omega^2r
```

```math
a_c=(26.2847)^2(0.050)
```

```math
\boxed{a_c=34.544\text{ m/s}^2}
```

In multiples of gravity:

```math
\frac{a_c}{g}
=
\frac{34.544}{9.81}
=
\boxed{3.52g}
```

---

# Motor and Encoder Mathematics

## 12. Encoder Resolution

Encoder:

```math
N_{CPR}=700\text{ counts/rev}
```

Radians per count:

```math
\theta_{count}
=
\frac{2\pi}{700}
=
\boxed{0.008976\text{ rad/count}}
```

Degrees per count:

```math
\theta_{count}
=
\frac{360}{700}
=
\boxed{0.5143\text{ °/count}}
```

At 251 rpm:

```math
f_{count}
=
\frac{251}{60}(700)
```

```math
\boxed{f_{count}=2928.3\text{ counts/s}}
```

Time between counts:

```math
\Delta t
=
\frac{1}{2928.3}
=
0.000341491\text{ s}
```

```math
\boxed{\Delta t=341.5\ \mu s}
```

---

## 13. Encoder Position and Speed

Angular position after `N` counts:

```math
\theta=N\left(\frac{2\pi}{700}\right)
```

Example: 100 counts:

```math
\theta
=
100(0.008976)
=
0.8976\text{ rad}
```

```math
\boxed{\theta=51.43°}
```

Angular speed estimate:

```math
\omega
\approx
\frac{\Delta N}{\Delta t}
\left(\frac{2\pi}{700}\right)
```

Example: 293 counts in 0.1 s:

```math
\omega
\approx
\frac{293}{0.1}(0.008976)
```

```math
\boxed{\omega\approx26.300\text{ rad/s}}
```

which is approximately the 251 rpm no-load speed.

---

## 14. Stall Torque and Equivalent Tangential Force

Maximum published/recorded stall torque:

```math
\tau_{stall}=1.77\text{ N·m}
```

Equivalent tangential force at the 50 mm flywheel radius:

```math
F=\frac{\tau}{r}
```

```math
F=\frac{1.77}{0.050}
```

```math
\boxed{F=35.4\text{ N}}
```

The documented motor bottom envelope diameter is approximately 39 mm:

```math
r=\frac{39}{2}=19.5\text{ mm}=0.0195\text{ m}
```

Equivalent force at that radius:

```math
F=\frac{1.77}{0.0195}
```

```math
\boxed{F=90.77\text{ N}}
```

This is an equivalent torque-reaction force, **not automatically the force on one screw**. Actual fastener load depends on the real mount geometry.

---

# Motor Electrical / Braking Mathematics

## 15. Estimated Motor Torque Constant

Using the approximate stall figures:

```math
K_t\approx\frac{\tau_{stall}}{I_{stall}}
```

```math
K_t\approx\frac{1.77}{7}
```

```math
\boxed{K_t\approx0.2529\text{ N·m/A}}
```

This is an estimate, not a measured motor constant.

---

## 16. Back-EMF at 251 rpm

Using the project's approximate:

```math
K_e=0.456\text{ V·s/rad}
```

```math
E=K_e\omega
```

```math
E=(0.456)(26.2847)
```

```math
\boxed{E\approx11.986\text{ V}}
```

This being close to 12 V is consistent with the motor approaching its no-load speed.

---

## 17. Approximate Dynamic-Braking Current

Using the simplified dynamic-braking model:

```math
I_b\approx\frac{K_e\omega}{R}
```

```math
I_b
=
\frac{0.456(26.2847)}{1.71}
```

```math
\boxed{I_b\approx7.009\text{ A}}
```

Approximate braking torque:

```math
\tau_b\approx K_tI_b
```

```math
\tau_b
=
(0.2529)(7.009)
```

```math
\boxed{\tau_b\approx1.772\text{ N·m}}
```

This simplified result is based on estimated motor constants and does not include driver current limiting, winding inductance, gearbox losses or the changing motor speed.

---

## 18. Reverse-Voltage Braking Current

The project's reverse-braking estimate uses:

```math
I_{reverse}
\approx
\frac{V+K_e\omega}{R}
```

Substitution:

```math
I_{reverse}
=
\frac{12+0.456(26.2847)}{1.71}
```

```math
I_{reverse}
=
\frac{12+11.986}{1.71}
```

```math
\boxed{I_{reverse}\approx14.03\text{ A}}
```

Approximate reverse torque:

```math
\tau_{reverse}
\approx
K_tI_{reverse}
```

```math
\tau_{reverse}
=
(0.2529)(14.03)
```

```math
\boxed{\tau_{reverse}\approx3.55\text{ N·m}}
```

Comparison with approximate motor stall current:

```math
\frac{14.03}{7}
=
\boxed{2.00\times}
```

So the simplified direct-reverse calculation predicts roughly **2.00 times the stated stall current**, supporting the decision not to use immediate full reverse braking.

---

## 19. Controlled PWM Deceleration in Current Code

Current code:

```math
t_{decel}=1000\text{ ms}=1.0\text{ s}
```

The PWM command is reduced linearly:

```math
PWM(t)=PWM_0\left(1-\frac{t}{1.0}\right)
```

### If braking starts at PWM = 200

| Time | Calculation | PWM |
|---:|---|---:|
| 0.00 s | `200(1 - 0/1)` | **200** |
| 0.25 s | `200(1 - 0.25/1)` | **150** |
| 0.50 s | `200(1 - 0.50/1)` | **100** |
| 0.75 s | `200(1 - 0.75/1)` | **50** |
| 1.00 s | `200(1 - 1/1)` | **0** |

### If braking starts at minimum tracking PWM = 110

| Time | Calculation | Ideal PWM before integer truncation |
|---:|---|---:|
| 0.00 s | `110(1 - 0/1)` | **110.0** |
| 0.25 s | `110(1 - 0.25/1)` | **82.5** |
| 0.50 s | `110(1 - 0.50/1)` | **55.0** |
| 0.75 s | `110(1 - 0.75/1)` | **27.5** |
| 1.00 s | `110(1 - 1/1)` | **0** |

The software makes PWM linear with time. **Motor angular speed is not guaranteed to decrease linearly with PWM**, so true angular deceleration still needs bench measurement.

---

# IR Detection / Control Mathematics

## 20. Sensor Sampling Rate

The documented approximate loop time is:

```math
t_{loop}
\approx
3(4.5\ \mu s)+100\ \mu s+3\ \mu s
```

```math
t_{loop}
\approx
\boxed{117\ \mu s}
```

Approximate samples per sensor in 100 ms:

```math
N_{100}
=
\frac{100000\ \mu s}{117\ \mu s}
```

```math
\boxed{N_{100}\approx854.7\approx855}
```

For threshold `K = 5`:

```math
Duty_{threshold}
=
\frac{5}{854.7}\times100
```

```math
\boxed{Duty_{threshold}\approx0.585\%}
```

Approximate LOW time represented by 5 loop samples:

```math
t_{LOW}
=
5(117\ \mu s)
=
\boxed{585\ \mu s=0.585\text{ ms}}
```

### 50 ms tracking window

```math
N_{50}
=
\frac{50000}{117}
=
\boxed{427.4\text{ samples}}
```

For threshold 3:

```math
Duty_{track}
=
\frac{3}{427.4}\times100
=
\boxed{0.702\%}
```

---

## 21. Count-Weighted Target Position

Current centroid equation:

```math
pos
=
\frac{D5-D4}{D4+D5+D6}
```

Range:

```math
-1\le pos\le+1
```

Example from the documented output style:

- `D4 = 5`
- `D5 = 40`
- `D6 = 60`

```math
pos
=
\frac{40-5}{5+40+60}
```

```math
pos
=
\frac{35}{105}
```

```math
\boxed{pos=+0.333}
```

Positive means the target is on the clockwise side of the centre sensor.

Perfect balance example:

- `D4 = 12`
- `D5 = 12`
- `D6 = 25`

```math
pos
=
\frac{12-12}{12+12+25}
=
\boxed{0}
```

---

## 22. Centroid Noise Estimate

The documented Poisson-style approximation is:

```math
\sigma_{pos}
\approx
\frac{\sqrt{D4+D5}}{D4+D5+D6}
```

For:

- `D4 = 12`
- `D5 = 12`
- `D6 = 25`

```math
\sigma_{pos}
\approx
\frac{\sqrt{12+12}}{12+12+25}
```

```math
=
\frac{\sqrt{24}}{49}
```

```math
=
\frac{4.899}{49}
```

```math
\boxed{\sigma_{pos}\approx0.1000\approx0.10}
```

---

## 23. PID Controller

Current gains:

```math
K_p=140,\qquad K_i=40,\qquad K_d=6
```

Controller:

```math
u=K_pe+K_iI+K_dD
```

Integral:

```math
I_{new}
=
clamp(I_{old}+e\Delta t,\pm1)
```

Derivative filter:

```math
D_{new}
=
0.8D_{old}
+
0.2\left(\frac{e-e_{prev}}{\Delta t}\right)
```

PWM:

```math
PWM=clamp(|u|,110,200)
```

### Numerical proportional example

For:

```math
e=0.40,\quad I=0,\quad D=0
```

```math
u
=
140(0.40)+40(0)+6(0)
```

```math
u=56
```

The raw controller output is 56, but the code enforces:

```math
PWM=clamp(56,110,200)
```

```math
\boxed{PWM=110}
```

So even a moderate error can command the minimum mechanically useful tracking PWM.

---

## 24. Widening Centre Deadband

Current code:

```math
dead(t)
=
0.15
+
0.45\min\left(1,\frac{t}{1.2}\right)
```

Calculated values:

| Time since turn started | Calculation | Deadband |
|---:|---|---:|
| 0.0 s | `0.15 + 0.45(0.0/1.2)` | **0.1500** |
| 0.1 s | `0.15 + 0.45(0.1/1.2)` | **0.1875** |
| 0.3 s | `0.15 + 0.45(0.3/1.2)` | **0.2625** |
| 0.6 s | `0.15 + 0.45(0.6/1.2)` | **0.3750** |
| 0.9 s | `0.15 + 0.45(0.9/1.2)` | **0.4875** |
| 1.2 s | `0.15 + 0.45(1)` | **0.6000** |

At `t = 1.2 s`:

```math
dead
=
0.15+0.45
=
\boxed{0.60}
```

---

## 25. Zero-Crossing Angular Resolution

The decision loop runs every:

```math
\Delta t=10\text{ ms}=0.010\text{ s}
```

At the documented low-end assumed tracking speed of `64°/s`:

```math
\Delta\theta
=
64(0.010)
=
\boxed{0.64°}
```

At `180°/s`:

```math
\Delta\theta
=
180(0.010)
=
\boxed{1.80°}
```

Therefore a sign change in `pos` locates the centre to roughly one 10 ms motion step, approximately **0.64–1.80°** for those assumed speeds.

---

## 26. Stop-Decision Timing

Components used by the current logic:

- one control slice: `10 ms`
- stop confirmation: `3 × 10 ms = 30 ms`
- rolling tracking window: `50 ms`

Simple total timing budget:

```math
t_{decision}
\approx
10+30+50
```

```math
\boxed{t_{decision}\approx90\text{ ms}}
```

The windows overlap, so this is a conservative accounting value rather than a guaranteed independent 90 ms delay.

---

# Sensor Geometry Mathematics

## 27. Three-Sensor Reference Geometry

For the archived/reference 3-sensor layout:

```math
s=\frac{360°}{3}
```

```math
\boxed{s=120°}
```

Assumed sensor field of view:

```math
\beta=90°
```

Static three-sensor coverage:

```math
C=3\beta
```

```math
C=3(90°)
```

```math
\boxed{C=270°}
```

Gap between adjacent sensor beams:

```math
g=s-\beta
```

```math
g=120°-90°
```

```math
\boxed{g=30°}
```

Maximum rotation to bring a target into a beam:

```math
\boxed{30°}
```

At `180°/s`:

```math
t
=
\frac{30°}{180°/s}
=
\boxed{0.1667\text{ s}}
```

At `60°/s`:

```math
t
=
\frac{30°}{60°/s}
=
\boxed{0.500\text{ s}}
```

---

## 28. Sensor-to-Centre Parallax

The documented projection equation is:

```math
\alpha_C
=
atan2(D\sin\alpha,\ R+D\cos\alpha)
```

Using:

- sensor radius `R = 50 mm = 0.050 m`
- sensor edge angle `α = 45°`

### Target at 1 m

```math
\alpha_C
=
atan2(1.0\sin45°,\ 0.050+1.0\cos45°)
```

```math
\boxed{\alpha_C\approx43.04°}
```

Parallax error:

```math
45°-43.04°
=
\boxed{1.96°}
```

### Target at 0.5 m

```math
\alpha_C
=
atan2(0.5\sin45°,\ 0.050+0.5\cos45°)
```

```math
\boxed{\alpha_C\approx41.22°}
```

Parallax error:

```math
45°-41.22°
=
\boxed{3.78°}
```

---

# Gyroscope Mathematics


## 29. Measured MPU6050 XYZ Values from Bench Output

The following calculations use the 19 visible IMU samples from the supplied serial-monitor screenshot.

### Measured mean acceleration

The accelerometer outputs are in units of `g`.

```math
\bar{A}_x = \frac{\sum A_x}{n}
```

```math
\bar{A}_x = \frac{-1.49}{19}
= \boxed{-0.0784\,g}
```

```math
\bar{A}_y = \frac{\sum A_y}{n}
```

```math
\bar{A}_y = \frac{13.89}{19}
= \boxed{+0.7311\,g}
```

```math
\bar{A}_z = \frac{\sum A_z}{n}
```

```math
\bar{A}_z = \frac{-13.15}{19}
= \boxed{-0.6921\,g}
```

Therefore the measured gravity/acceleration vector in the MPU6050 sensor frame is approximately:

```math
\boxed{\vec A = (-0.0784,\;0.7311,\;-0.6921)\,g}
```

### Resultant acceleration magnitude

```math
|\vec A|
=
\sqrt{A_x^2+A_y^2+A_z^2}
```

```math
|\vec A|
=
\sqrt{(-0.0784)^2+(0.7311)^2+(-0.6921)^2}
```

```math
|\vec A|
=
\sqrt{0.00615+0.5344+0.4790}
```

```math
|\vec A|
=
\sqrt{1.0196}
```

```math
\boxed{|\vec A|=1.0098\,g}
```

Convert to SI acceleration:

```math
a = 1.0098(9.81)
```

```math
\boxed{a=9.906\text{ m/s}^2}
```

This is close to the expected gravitational acceleration of \(1g\), which is consistent with the accelerometer behaving plausibly during the captured period.

### Accelerometer sample variation

Sample standard deviations from the 19 visible readings:

```math
\boxed{\sigma_{A_x}\approx0.0201\,g}
```

```math
\boxed{\sigma_{A_y}\approx0.0115\,g}
```

```math
\boxed{\sigma_{A_z}\approx0.0187\,g}
```

The mean resultant magnitude across the visible samples is approximately:

```math
\boxed{\bar{|A|}\approx1.0101\,g}
```

with a sample-to-sample standard deviation of approximately:

```math
\boxed{\sigma_{|A|}\approx0.0132\,g}
```

### Sensor-frame tilt from gravity vector

The angle between the measured acceleration vector and the positive \(Y\) axis is:

```math
\theta_Y
=
\cos^{-1}\left(\frac{A_y}{|\vec A|}\right)
```

```math
\theta_Y
=
\cos^{-1}\left(\frac{0.7311}{1.0098}\right)
```

```math
\boxed{\theta_Y\approx43.61^\circ}
```

The angle between the measured gravity vector and the negative \(Z\) axis is:

```math
\theta_{-Z}
=
\cos^{-1}\left(\frac{-A_z}{|\vec A|}\right)
```

```math
\theta_{-Z}
=
\cos^{-1}\left(\frac{0.6921}{1.0098}\right)
```

```math
\boxed{\theta_{-Z}\approx46.73^\circ}
```

The small \(X\)-axis component corresponds to:

```math
\theta_X^\prime
=
\tan^{-1}
\left(
\frac{|A_x|}
{\sqrt{A_y^2+A_z^2}}
\right)
```

```math
\theta_X^\prime
=
\tan^{-1}
\left(
\frac{0.0784}
{\sqrt{0.7311^2+0.6921^2}}
\right)
```

```math
\boxed{\theta_X^\prime\approx4.45^\circ}
```

So the captured MPU6050 orientation has gravity mainly split between \(+Y\) and \(-Z\), with only a small \(X\)-axis component.

### Measured mean gyroscope values

The 19 visible samples give:

```math
\bar{\omega}_x
=
\frac{-34.3}{19}
=
\boxed{-1.805\text{ °/s}}
```

```math
\bar{\omega}_y
=
\frac{65.1}{19}
=
\boxed{+3.426\text{ °/s}}
```

```math
\bar{\omega}_z
=
\frac{2.5}{19}
=
\boxed{+0.132\text{ °/s}}
```

The magnitude of the mean angular-rate vector is:

```math
|\bar{\omega}|
=
\sqrt{(-1.805)^2+(3.426)^2+(0.132)^2}
```

```math
\boxed{|\bar{\omega}|\approx3.875\text{ °/s}}
```

Sample standard deviations are approximately:

```math
\boxed{\sigma_{\omega_x}\approx3.54\text{ °/s}}
```

```math
\boxed{\sigma_{\omega_y}\approx4.17\text{ °/s}}
```

```math
\boxed{\sigma_{\omega_z}\approx3.88\text{ °/s}}
```

These raw gyroscope values include sensor bias/noise because the current output code prints the measured gyro values without a completed bias-removal calibration.

### Important distinction: IMU XYZ is not the centre-of-mass XYZ

The values above are:

- accelerometer readings \(A_x,A_y,A_z\), in `g`;
- gyroscope readings \(\omega_x,\omega_y,\omega_z\), in degrees per second.

They are **not physical component coordinates** such as \(x_i,y_i,z_i\) in millimetres from the satellite centre. Therefore they cannot be substituted into:

```math
x_{CG}=\frac{\sum m_ix_i}{\sum m_i},
\quad
y_{CG}=\frac{\sum m_iy_i}{\sum m_i},
\quad
z_{CG}=\frac{\sum m_iz_i}{\sum m_i}
```

To calculate the actual centre of gravity numerically, the final CAD or measured position of each component is still required.

---

## 30. MPU6050 Scale

Current configuration:

```math
LSB=32.8\text{ counts/(°/s)}
```

Angular-rate resolution per raw count:

```math
\Delta\omega
=
\frac{1}{32.8}
```

```math
\boxed{\Delta\omega=0.03049\text{ °/s per count}}
```

Maximum configured rate:

```math
\omega_{max}=1000°/s
```

Convert to rpm:

```math
N
=
1000\frac{60}{360}
```

```math
\boxed{N=166.67\text{ rpm}}
```

Gyro integration:

```math
\theta_{gyro}
\leftarrow
\theta_{gyro}+\omega\Delta t
```

At `100 Hz`:

```math
\Delta t=\frac1{100}=\boxed{0.010\text{ s}}
```

If residual bias is `0.05°/s` over 5 s:

```math
\theta_{drift}
=
0.05(5)
=
\boxed{0.25°}
```

A documented rectangular-integration braking error estimate at `180°/s` is:

```math
e
\lesssim
\frac12\omega\Delta t
```

```math
e
\lesssim
\frac12(180)(0.010)
```

```math
\boxed{e\lesssim0.90°}
```

---

# Laser Timing Mathematics

## 31. Laser Hold Margin

Current code holds the laser for:

```math
t_{laser}=4000\text{ ms}=4.0\text{ s}
```

Project minimum requirement:

```math
t_{required}=2.0\text{ s}
```

Timing margin:

```math
Margin
=
4.0-2.0
=
\boxed{2.0\text{ s}}
```

Ratio:

```math
\frac{4.0}{2.0}
=
\boxed{2.0}
```

So the programmed laser hold is **200% of the minimum required duration**.

---

# Structural / Load-Bearing Checks

## 32. Torque Reaction at the Motor Mount

The motor mount must react the motor torque:

```math
\tau = Fr
```

Therefore:

```math
F=\frac{\tau}{r}
```

Using stall torque as an extreme upper-bound torque:

```math
\tau=1.77\text{ N·m}
```

Using the documented 39 mm bottom motor envelope:

```math
r=19.5\text{ mm}=0.0195\text{ m}
```

```math
F
=
\frac{1.77}{0.0195}
=
\boxed{90.77\text{ N}}
```

Using the 35 mm gearbox body diameter:

```math
r=17.5\text{ mm}=0.0175\text{ m}
```

```math
F
=
\frac{1.77}{0.0175}
=
\boxed{101.14\text{ N}}
```

These are **equivalent tangential reaction forces** at those radii. They are useful for comparing the torque scale with the static weight of only `10.643 N`.

Torque-equivalent force / static known weight:

```math
\frac{90.77}{10.643}
=
\boxed{8.53\times}
```

So the extreme stall-torque reaction scale can be several times larger than the satellite's current static gravitational load.

---

## 33. Bending Stress Formula for the 3D-Printed Mount

For a cantilever-style rectangular bracket:

```math
M_b=FL
```

```math
I=\frac{bh^3}{12}
```

```math
\sigma_b=\frac{Mc}{I}
```

with:

```math
c=\frac{h}{2}
```

Therefore:

```math
\boxed{\sigma_b=\frac{6FL}{bh^2}}
```

A final numerical bending stress **cannot be calculated from the current project files** because final bracket:

- unsupported length `L`
- width `b`
- thickness `h`
- material/print-direction allowable stress

are not recorded numerically.

Do not invent these values for the report. Measure them from the final CAD and substitute them here.

---

## 34. Bracket Deflection Formula

For a cantilever:

```math
\boxed{\delta=\frac{FL^3}{3EI}}
```

For a rectangular section:

```math
I=\frac{bh^3}{12}
```

Final deflection cannot be evaluated without the final bracket dimensions and an appropriate effective elastic modulus for the actual printed material/orientation.

---

## 35. Laser Pointing Error from Bracket Deflection

If the laser mount deflects laterally by `δ` over support length `L`:

```math
\theta_{flex}
=
\tan^{-1}\left(\frac{\delta}{L}\right)
```

For small angles:

```math
\theta_{flex}\approx\frac{\delta}{L}
```

At target distance `D`:

```math
x_{target}\approx D\theta_{flex}
```

The final numerical value requires the measured/calculated mount deflection.

---

# Values Still Required to Close the Final Mechanical Calculations

The following values are **not present in the current project files**, so no defensible final numerical result can be produced for them yet:

| Missing value | Needed for |
|---|---|
| Final full satellite mass | final suspension / load-bearing check |
| Final component `x, y, z` coordinates from CAD | actual centre of mass |
| PVC mass | actual total mass and inertia |
| Laser + sensor + wiring masses | actual total mass and CG |
| Final 3D-printed chassis/mount mass | total mass and CG |
| Final radial positions of major components | satellite moment of inertia |
| Bracket unsupported length `L` | bending / deflection |
| Bracket width `b` | bending / deflection |
| Bracket thickness `h` | bending / deflection |
| Printed material and print orientation | allowable stress / effective `E` |
| Actual measured chassis angular speed vs PWM | real stopping distance |
| Measured deceleration during the 1 s PWM ramp | real braking torque |
| Final suspension-line angle | exact line tension |

Once these are measured, use the formulas already included above rather than replacing them.

---

# Quick Results Summary

| Result | Calculated value |
|---|---:|
| Current known mass | **1.0849 kg** |
| Current static weight | **10.643 N** |
| Two vertical support load | **5.321 N each** |
| Flywheel calculated mass from geometry/density | **0.6165 kg** |
| Flywheel inertia | **0.000770 kg·m²** |
| Motor max output speed | **26.2847 rad/s** |
| Flywheel angular momentum at 251 rpm | **0.02024 N·m·s** |
| Flywheel kinetic energy at 251 rpm | **0.2660 J** |
| Encoder resolution | **0.5143°/count** |
| Encoder count rate at 251 rpm | **2928.3 counts/s** |
| Approx. reverse-braking current | **14.03 A** |
| Approx. reverse-braking torque | **3.55 N·m** |
| 100 ms sensor samples | **≈ 855** |
| Detection threshold equivalent duty | **≈ 0.585%** |
| Example target centroid `(5,40,60)` | **+0.333** |
| Example centroid noise `(12,12,25)` | **σ ≈ 0.1000** |
| Centre deadband after 1.2 s | **0.60** |
| Current PWM deceleration duration | **1.0 s** |
| Current laser hold | **4.0 s** |
| Laser hold / minimum requirement | **2.0×** |
| Extreme stall-torque equivalent force at 19.5 mm | **90.77 N** |

