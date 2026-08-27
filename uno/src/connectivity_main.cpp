// ============================================================
// COMPUTER CONNECTIVITY TEST (no motor)
// Build/upload with: pio run -e connectivity -t upload
// Then:              pio device monitor -e connectivity
//
// Purpose: confirms the board is powered, running code, and
// successfully talking to your PC over USB serial. Nothing else
// needs to be wired for this test.
// ============================================================
#include <Arduino.h>

const int LED_PIN = 13; // onboard LED

unsigned long heartbeatCount = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println("=== Connectivity Test ===");
  Serial.println("If you can read this, the board is connected and running code.");
  Serial.println();
}

void loop() {
  heartbeatCount++;

  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);

  Serial.print("Heartbeat #");
  Serial.print(heartbeatCount);
  Serial.println(" - board connected and responding");

  delay(800);
}
