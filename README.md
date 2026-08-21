# engg2000_2026
engg2000_S2 2026


Team name: Artemis III

Satellite name: Buzz Lightyear

Name Specialisation/s

- Pranaya Mechatronics
  
- Pratham Electronics
  
- Minying Wu Software
  
- Bhaavna Software
  
- Arpit Mechatronics

## Software

### motor_onoff — continuous spin, stoppable with spacebar:

- pio run -e motor_onoff -t upload
- pio device monitor -e motor_onoff

Type g + Enter to start — it'll spin continuously and print a status line every second (Status: RUNNING | speed=255 | elapsed=4s...). Type a space + Enter at any time to stop it.

### motor_flag_sequence — your third file, running as-is (just with the pinMode ordering fixed and status prints added):

- pio run -e motor_flag_sequence -t upload
- pio device monitor -e motor_flag_sequence

This one runs automatically on power-up/upload — forward 5s → stop 2s → reverse 5s → stop 2s → done. No serial input needed; it's a one-shot test that halts after one cycle (matching your hasRun flag logic).

### connection_test_main - Motor Connectivity Test

- pio device monitor -e connection_test

(Or in VS Code: click the plug icon at the bottom of the PlatformIO toolbar, or PROJECT TASKS → connection_test → Monitor.)

You should see it print, repeating every ~6 seconds:

=== Nano <-> Motor Connectivity Test ===
Reply: 1 = motor confirmed connected, 0 = not connected
FORWARD: 1  (connected - encoder moved)
REVERSE: 1  (connected - encoder moved)


