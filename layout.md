Layout of Arduino, DRV8874, and Encoder connections:
                   +---------------- Arduino Nano ----------------+
                   |                                              |
Encoder VCC -------+ 5V                                           |
Encoder GND -------+ GND ----------------------+                  |
Encoder A ---------+ D2                        |                  |
Encoder B ---------+ D3                        |                  |
                                               |                  |
DRV PH/IN2 <-------+ D8                        |                  |
DRV EN/IN1 <-------+ D9 (PWM)                  |                  |
DRV SLEEP <--------+ 5V                        |                  |
                                               |                  |
                   +---------------------------+------------------+
                                               |
                                             COMMON
                                              GND
                                               |
                    +---------------- DRV8874 --+-------------+
                    |                                        |
12V PSU + ----------+ VIN                                OUT1 +---- Motor
12V PSU - ----------+ GND                                OUT2 +---- Motor
                    |
                    + PMODE ---- GND
