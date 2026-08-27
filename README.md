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
Blinks the laser diode ON 1s / OFF 1s. Wired to pin 6 (not 8, which is
used by the motor's DIR signal).

```
pio run -e laser_test -t upload
pio device monitor -e laser_test
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
| Motor PWM | D9 |
| Motor DIR | D8 |
| Motor nSLEEP | D7 |
| Encoder A | D2 |
| Encoder B | D3 |
| IR sensor (reactive_ir) | D2 |
| Laser | D6 |

Note: `connection_test` uses D2/D3 for the encoder; `reactive_ir` uses
D2 for the IR receiver. These are different environments/sketches and
never run at the same time, so there's no actual pin conflict - just
don't wire both encoder and IR receiver to D2 simultaneously if you
combine them into one sketch later.

### IRSensor.cpp / IRSensor.h
Kept in the project as the original multi-sensor IR ring class (from
the full system design), but not currently wired into any environment
above. This is reference code for when the full sensor ring is built -
see `include/Config.h` for the IR pin array it expects.

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

### Pin reference (Uno R3)
| Signal | Pin |
|---|---|
| Motor PWM | D9 |
| Motor DIR | D8 |
| Motor nSLEEP | D7 |
| IR sensor | D2 |
| White LED | D4 |
| Green LED | D5 |
