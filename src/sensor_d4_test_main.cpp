// ============================================================
// SENSOR D4 TEST - checks that ONE IR sensor (TSOP4138 on D4) works - Nano V3
// Build/upload with: pio run -e sensor_d4_test -t upload
// Then:              pio device monitor -e sensor_d4_test     (115200 baud)
//
// Sensor only - NO motor, NO laser, NO driver: the program never touches the
// motor or laser pins. Wiring: TSOP4138 VCC -> 5V, GND -> GND, OUT -> D4.
// Nothing else needs to be connected.
//
// What it shows (every 100 ms):
//   D4 level: HIGH|LOW   LOW reads: 72/850   edges: 14   DETECTED|--
// The TSOP output is HIGH when it sees nothing and goes LOW while it sees
// the 38 kHz message. The onboard LED (D13) lights while D4 is LOW.
//
// Every 5 s a verdict is printed:
//   WORKING     - D4 changes between HIGH and LOW
//   NO SIGNAL   - D4 stays HIGH (point the beacon at the sensor / check wiring)
//   STUCK LOW   - D4 is almost always LOW (output shorted, wrong pin, or no power)
// ============================================================
#include <Arduino.h>

const int SENSOR_PIN = 4;         // TSOP4138 OUT, active-LOW
const int LED_PIN    = 13;        // onboard LED mirrors the sensor

const unsigned long WINDOW_MS = 100;
const int DETECT_THRESHOLD = 5;               // LOW reads per window = message (spin_until_ir value)
const unsigned long VERDICT_MS = 5000;
const int STUCK_PERCENT = 95;                 // LOW this % of the time = stuck

unsigned long totalSamples = 0;
unsigned long totalLow = 0;
unsigned long totalEdges = 0;
unsigned long detectedWindows = 0;
unsigned long windows = 0;
unsigned long lastVerdictMs = 0;
int lastLevel = HIGH;

void setup() {
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println(F("=== Sensor D4 test (TSOP4138 OUT on D4, sensor only - no motor, no laser) ==="));
  delay(100);
  lastLevel = digitalRead(SENSOR_PIN);
  Serial.print(F("Idle level of D4 right now: "));
  Serial.println(lastLevel == HIGH ? F("HIGH (normal when no message is seen)")
                                   : F("LOW (a message is on it, or the pin is stuck LOW)"));
  Serial.println(F("Point the IR beacon at the sensor. Expect: LOW reads go up and DETECTED appears."));
  Serial.println();
  lastVerdictMs = millis();
}

void loop() {
  unsigned long start = millis();
  unsigned long samples = 0, low = 0, edges = 0;

  while (millis() - start < WINDOW_MS) {
    int level = digitalRead(SENSOR_PIN);
    samples++;
    if (level == LOW) low++;
    if (level != lastLevel) {
      edges++;
      lastLevel = level;
    }
    digitalWrite(LED_PIN, level == LOW ? HIGH : LOW);
    delayMicroseconds(100);
  }

  totalSamples += samples;
  totalLow += low;
  totalEdges += edges;
  windows++;
  bool detected = low >= (unsigned long)DETECT_THRESHOLD;
  if (detected) detectedWindows++;

  Serial.print(F("D4 level: "));
  Serial.print(lastLevel == HIGH ? F("HIGH") : F("LOW "));
  Serial.print(F("   LOW reads: "));
  Serial.print(low);
  Serial.print('/');
  Serial.print(samples);
  Serial.print(F("   edges: "));
  Serial.print(edges);
  Serial.print(F("   "));
  Serial.println(detected ? F("DETECTED") : F("--"));

  if (millis() - lastVerdictMs >= VERDICT_MS) {
    lastVerdictMs = millis();
    unsigned long lowPercent = totalSamples ? (totalLow * 100UL) / totalSamples : 0;

    Serial.print(F(">>> VERDICT after "));
    Serial.print(windows / 10);
    Serial.print(F(" s: "));
    if (lowPercent >= (unsigned long)STUCK_PERCENT) {
      Serial.println(F("STUCK LOW - D4 is almost always LOW. Check: OUT really on D4, VCC 5V and GND connected, no short to GND, sensor not reversed."));
    } else if (totalLow == 0) {
      Serial.println(F("NO SIGNAL - D4 stayed HIGH. Point the 38 kHz beacon at the sensor. If it never changes, check OUT -> D4, VCC 5V, GND."));
    } else {
      Serial.print(F("WORKING - D4 changes state ("));
      Serial.print(totalEdges);
      Serial.print(F(" edges, "));
      Serial.print(detectedWindows);
      Serial.print(F(" of "));
      Serial.print(windows);
      Serial.println(F(" windows detected the message)."));
    }
    totalSamples = totalLow = totalEdges = 0;
    detectedWindows = windows = 0;
  }
}
