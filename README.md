# MQ Sentinel - ENGG2000 Space Debris Cleanup Challenge

Team: Artemis III | Satellite: Buzz Lightyear

This project has two separate boards, each with its own self-contained
PlatformIO project:

- **Root folder** (`platformio.ini` here): **Arduino Nano V3** - the
  original supplied board. The first Nano had a faulty auto-reset
  circuit and has been replaced; this is the clean rebuild for the
  replacement board.
- **`uno/` folder**: **Arduino Uno R3** - used during development
  because its genuine USB-serial chip gave far more reliable uploads
  than the Nano clone while debugging hardware issues.

Open whichever board's folder you're working with directly in VS Code
(PlatformIO reads the `platformio.ini` in the folder you open).

---

## Nano V3 (this folder)

### motor_onoff
g = start continuous spin | space = stop | r = reverse direction and
keep spinning continuously. **Auto-starts spinning immediately on
power-up** - works standalone off battery power with no USB needed.
If USB IS connected, g/space/r still work as normal on top of that.

```
pio run -e motor_onoff -t upload
pio device monitor -e motor_onoff
```

### ir_triggered_spin
Pipeline test: pairs with the Uno's `ir_transmitter_test`. Motor stays
off until the IR receiver detects the transmitted 38kHz signal - once
detected (even briefly), the motor triggers and spins continuously
from then on, regardless of whether the signal is still present. This
is a one-way latch, not a live on/off toggle - reset the board to
re-arm it.

```
pio run -e ir_triggered_spin -t upload
pio device monitor -e ir_triggered_spin
```

### connection_test
Confirms the Nano is actually driving the motor by pulsing it briefly
and checking encoder counts before/after. Reply: `1` = connected,
`0` = not connected.

```
pio run -e connection_test -t upload
pio device monitor -e connection_test
```

### laser_test
Blinks the laser diode ON 1s / OFF 1s. Wired to pin 10 (pins 4/5/6 are
now used by the IR array, 8/9 by the motor).

```
pio run -e laser_test -t upload
pio device monitor -e laser_test
```

### directional_ir
Samples a ring of six IR receivers spaced 60 degrees apart (360/6 = 60:
left/right/centre on D4/D5/D6 plus rear-right/rear/rear-left on
A0/A1/A2) and turns the motor toward whichever sensor has the
strongest signal; dead-ahead (centre) or no signal stops the motor,
right-side sensors turn right, left-side sensors turn left. Fix
applied: the original snippet didn't enable the DRV8874's nSLEEP pin,
which is required for the motor to spin at all - added back in.

```
pio run -e directional_ir -t upload
pio device monitor -e directional_ir
```

### ir_sensor_test (v0.1)
Prints LOW-read counts for three TSOP4138 receivers (S1=D4, S2=D5,
S3=D6) over 100 ms windows. Motor driver is held asleep. Use it to
choose the detection threshold.

```
pio run -e ir_sensor_test -t upload
pio device monitor -e ir_sensor_test
```

### spin_until_ir (v0.2)
Motor spins continuously and stops when any of the three sensors
detects the 38 kHz signal; resumes once the signal has been clear for
two windows (set `LATCH_ON_DETECT = true` to stay stopped). Maths and
tuning: [IR_Spin_Stop_Math.md](IR_Spin_Stop_Math.md).

```
pio run -e spin_until_ir -t upload
pio device monitor -e spin_until_ir
```

### reactive_ir
Motor spins by default; stops the moment the IR receiver detects a
signal, resumes automatically when the signal clears.

```
pio run -e reactive_ir -t upload
pio device monitor -e reactive_ir
```

### Pin reference (Nano V3)
| Signal | Pin |
|---|---|
| Motor DIR | D8 |
| Motor PWM | D9 |
| Motor nSLEEP | D7 |
| IR Left | D4 |
| IR Right | D5 |
| IR Centre | D6 |
| IR Rear-Right (directional_ir only) | A0 |
| IR Rear (directional_ir only) | A1 |
| IR Rear-Left (directional_ir only) | A2 |
| Encoder A (connection_test only) | D2 |
| Encoder B (connection_test only) | D3 |
| Laser | D10 |

`reactive_ir` and `ir_triggered_spin` use a single sensor on D6
(matching IR_CENTRE), so the same physical sensor position works
across every IR-related test without rewiring.

### IRSensor.cpp / IRSensor.h
Kept in the project as the original multi-sensor IR ring class (from
the full system design), but not currently wired into any environment
above. `include/Config.h`'s pin array now matches the same
left/right/centre convention: {4, 5, 6}.

### If uploads fail
See the troubleshooting notes at the top of `platformio.ini`. In
short: check nothing is wired to D0/D1, disconnect battery power from
VIN during upload, and try switching the bootloader variant if needed.

---

## Uno R3 (`uno/` folder)

Open the `uno/` folder in VS Code separately (it has its own
`platformio.ini`).

### motor_control
g = start | space = stop | r = reverse direction (applies immediately
if running, or on next start if stopped)

```
pio run -e motor_control -t upload
pio device monitor -e motor_control
```

### motor_autospin
Starts spinning clockwise the instant power is applied - no PC or
commands needed.

```
pio run -e motor_autospin -t upload
pio device monitor -e motor_autospin
```

### motor_ir_led
Motor spins by default, stops when IR detects a signal. White LED =
IR detected, green LED = motor spinning (should always be opposite).

```
pio run -e motor_ir_led -t upload
pio device monitor -e motor_ir_led
```

### connectivity
Pure board<->PC connectivity check, no motor wiring needed.

```
pio run -e connectivity -t upload
pio device monitor -e connectivity
```

### ir_transmitter_test
Generates a genuine 38kHz square wave on pin D3 via Timer2 hardware
PWM - drives an IR LED as a beacon transmitter, matching your
project's 38kHz modulation spec. This signal is invisible to the eye
(38,000 flashes/second), so a separate visible status LED (D5) lights
up solid to confirm the transmitter is active, and the onboard LED
blinks as a heartbeat to confirm the sketch hasn't frozen.

```
pio run -e ir_transmitter_test -t upload
pio device monitor -e ir_transmitter_test
```

Wiring: D3 -> IR LED (with current-limiting resistor). D5 -> resistor
-> visible status LED -> GND.

### mpu6050_gyro
Reads accel/gyro/temp from an MPU6050 (GY-521) breakout and prints it
over serial. Uses software (bit-banged) I2C on D8 (SDA) / D9 (SCL),
since the Uno's hardware I2C is fixed to A4/A5 - no external I2C
library required. D8/D9 are also the motor DIR/PWM pins on every other
env above, so run this on its own board (not wired to the motor at the
same time).

Wiring: VCC -> 5V, GND -> GND, SDA -> D8, SCL -> D9, AD0 -> GND.

```
pio run -e mpu6050_gyro -t upload
pio device monitor -e mpu6050_gyro
```

### Pin reference (Uno R3)
| Signal | Pin |
|---|---|
| Motor PWM | D9 |
| Motor DIR | D8 |
| Motor nSLEEP | D7 |
| IR sensor | D2 |
| White LED | D4 |
| Green LED | D5 |

Testing result: 
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
Accel(g) X: 0.044 Y: -0.190 Z: 0.970   Gyro(X: 0.057 Y: -0.192 Z: 0.972   Gyro(deg/s) X: -1.47 Y: 0.42 Z: -1.37   Temp: 26.6 C
Accel(g) X: 0.044 Y: -0.190 Z: 0.970   Gyro(MPU6050 initialized.
Accel(g) X: 0.110 Y: -0.190 Z: 1.033   Gyro(deg/s) X: 2.23 Y: -22.60 Z: 11.11   Temp: 26.6 C
Accel(g) X: 0.533 Y: -0.031 Z: 0.790   Gyro(deg/s) X: 27.49 Y: -80.64 Z: -15.28   Temp: 26.6 C
Accel(g) X: 0.829 Y: 0.120 Z: 0.627   Gyro(deg/s) X: -0.66 Y: -9.31 Z: -3.56   Temp: 26.6 C
Accel(g) X: 0.862 Y: 0.151 Z: 0.540   Gyro(deg/s) X: -1.45 Y: -10.10 Z: -2.28   Temp: 26.6 C
Accel(g) X: 0.828 Y: 0.114 Z: 0.633   Gyro(deg/s) X: -6.18 Y: 11.16 Z: 2.44   Temp: 26.6 C
Accel(g) X: 0.775 Y: 0.079 Z: 0.659   Gyro(deg/s) X: -6.87 Y: 9.41 Z: 3.43   Temp: 26.6 C
Accel(g) X: 0.563 Y: -0.166 Z: 0.771   Gyro(deg/s) X: -18.89 Y: 27.92 Z: 18.66   Temp: 26.6 C
Accel(g) X: 0.476 Y: -0.295 Z: 0.855   Gyro(deg/s) X: -10.85 Y: 7.82 Z: 8.22   Temp: 26.6 C
Accel(g) X: 0.474 Y: -0.318 Z: 0.827   Gyro(deg/s) X: -3.81 Y: 3.50 Z: 0.47   Temp: 26.6 C


-------------


## Code explain

### motor_onoff_main.cpp
Controls the motor manually or automatically. Three functions: startMotor() turns the driver on at full speed in the current direction; stopMotor() cuts power; reverseAndKeepSpinning() flips direction and keeps spinning continuously. setup() auto-starts the motor immediately on power-up (so it works standalone off battery, no USB needed), then loop() listens for optional serial commands (g=start, space=stop, r=reverse) if a computer happens to be connected.

Arduino Nano

    │
    ├── Pin 9 ──> PWM ──> Motor Driver ──> Motor speed

    │
    ├── Pin 8 ──> DIR ──> Motor Driver ──> Motor direction

    │
    └── Pin 7 ──> SLEEP ─> Motor Driver ──> Wake/Sleep
    
Power ON

   ↓
Wake motor driver

   ↓
Set direction = clockwise

   ↓
Set speed = 255

   ↓
Motor starts spinning

- HIGH → wake driver
- LOW  → sleep driver

- Speed 
  - 0   = motor off
  - 64  = roughly 25%
  - 128 = roughly 50%
  - 192 = roughly 75%
  - 255 = 100%

### connection_test_main.cpp
A diagnostic, not a real feature — proves the Nano is actually driving the motor. encoderISR() is an interrupt handler that counts motor shaft rotation via the encoder. pulseMotor() briefly spins the motor one direction. runDirectionTest() records the encoder count before and after a pulse — if it changed, prints 1 (motor confirmed connected); if not, prints 0. loop() repeats this in both directions every few seconds.

### laser_test_main.cpp
The simplest file — just blinks the laser diode on pin D10 on/off every second, with status printed to Serial, to confirm the laser wiring works in isolation.

### reactive_ir_main.cpp
Motor spins by default; startMotor()/stopMotor() toggle it based on whether the IR sensor (pin D6) currently detects a signal. loop() continuously checks the sensor and reacts live — motor stops the instant a signal appears, resumes the instant it's gone.

### directional_ir_main.cpp
The most complex one — reads three IR sensors (left/right/centre) for 50ms, counts how many times each read "detected," and steers the motor toward whichever side had the strongest signal (or stops if centre wins or nothing's detected). This is a basic proportional-ish steering test, not the final targeting logic.

### ir_triggered_spin_main.cpp
A one-way "trigger" — motor stays off until the IR sensor detects a signal even once, then it starts spinning and never stops (a latch, not a live toggle). Meant to pair with the Uno's IR transmitter to test a simple send-signal → motor-reacts pipeline.

### IRSensor.cpp (+ include/IRSensor.h)
A more built-out C++ class version of IR sensing (not a standalone sketch — nothing currently uses it). begin() sets up the pins; update() scans all sensors each loop and records which one is active; beaconDetected() returns whether anything's currently triggered; getBearingEstimate() gives a rough angle guess based on which sensor fired. This is scaffolding for a future multi-sensor version, currently unused by any of the six sketches above.
