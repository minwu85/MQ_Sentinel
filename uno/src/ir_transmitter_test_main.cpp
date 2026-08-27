// ============================================================
// 38kHz IR TRANSMITTER TEST - with visible status LED
// Build/upload with: pio run -e ir_transmitter_test -t upload
// Then:              pio device monitor -e ir_transmitter_test
//
// Pin 3 outputs a genuine 38kHz square wave via Timer2 hardware
// PWM - this drives an IR LED as a beacon transmitter, matching
// the 38kHz modulation your project spec requires. This signal
// is INVISIBLE to the naked eye (and to most phone cameras) since
// it flashes 38,000 times per second - that's expected, not a bug.
//
// STATUS_LED_PIN is a separate, ordinary visible LED that lights
// up solid to confirm the sketch is running and the 38kHz signal
// is actively being generated, since you can't see pin 3 directly.
// The onboard LED also blinks slowly as a heartbeat.
//
// WIRING:
//   Pin 3 -> IR LED (through appropriate current-limiting resistor)
//   STATUS_LED_PIN -> resistor -> visible LED anode, cathode -> GND
// ============================================================
#include <Arduino.h>

const int STATUS_LED_PIN = 5; // visible LED - confirms transmitter is active
const int HEARTBEAT_LED_PIN = 13; // onboard LED - confirms sketch is running

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(3, OUTPUT); // IR LED output - driven directly by Timer2 hardware below
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(HEARTBEAT_LED_PIN, OUTPUT);

  // Configure Timer2 to generate a 38kHz PWM signal on Pin 3.
  // This hardware-level configuration bypasses regular analogWrite
  // limits, giving a precise 38kHz carrier for IR beacon transmission.
  TCCR2A = _BV(COM2B1) | _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);
  OCR2A = 210; // sets frequency to ~38kHz
  OCR2B = 105; // 50% duty cycle (square wave pulse)

  // Turn the visible status LED ON solid - this is your confirmation
  // that setup() completed and the 38kHz timer is now running,
  // even though pin 3's actual output is invisible to you.
  digitalWrite(STATUS_LED_PIN, HIGH);

  Serial.println("=== 38kHz IR Transmitter Test ===");
  Serial.println("Pin 3 is now outputting a 38kHz carrier (invisible to the eye).");
  Serial.println("Status LED (solid ON) confirms the timer is running.");
  Serial.println("Heartbeat LED blinks to confirm the sketch is alive.");
}

void loop() {
  // The hardware timer runs constantly in the background on pin 3 -
  // nothing needed here to keep the 38kHz signal going. This loop
  // just blinks the heartbeat LED so you have ongoing visual proof
  // the board hasn't frozen or reset.
  digitalWrite(HEARTBEAT_LED_PIN, HIGH);
  delay(500);
  digitalWrite(HEARTBEAT_LED_PIN, LOW);
  delay(500);
}
