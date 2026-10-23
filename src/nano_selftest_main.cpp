// ============================================================
// NANO V3 SELF-TEST - confirms that the board itself works - Nano V3
// Build/upload with: pio run -e nano_selftest -t upload
// Then:              pio device monitor -e nano_selftest     (115200 baud)
//
// Needs NO other hardware: nothing has to be connected. It tests the
// functions of the ATmega328P board and prints PASS / FAIL / INFO:
//   1 chip signature      2 free RAM           3 supply voltage (ADC bandgap)
//   4 onboard LED D13     5 EEPROM write/read  6 timer interrupt (Timer1, 1 kHz)
//   7 pull-ups + outputs on the FREE pins D11 D12 A0 A1 A2 A3
//   8 ADC on A0..A3 (driven HIGH / LOW)        9 PWM on D11 (3 duty cycles)
//  10 I2C bus (A4/A5) + scan for the MPU6050 (0x68)   11 clock tick check
//  12 optional jumper loop-back D11 -> D12     13 project pins D2..D6 (read only)
//
// SAFE BY DEFAULT: the motor-driver pins (D7 D8 D9), the laser pin (D10)
// and the serial pins (D0 D1) are never driven, so the motor cannot move.
// Optional (set the flags below): laser pulse, motor + encoder test.
//
// Serial command: r = run all tests again.
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <avr/boot.h>
#include <avr/interrupt.h>

// ---------- Options ----------
const bool TEST_LASER_PULSE = false;      // true: pulse the laser on D10 for 300 ms
const bool TEST_MOTOR = false;            // true: the MOTOR WILL TURN (1 s each way) and the encoder on D2/D3 is read
const int MOTOR_TEST_PWM = 130;

// ---------- Pins ----------
const int LED_PIN = 13;
const int MOTOR_SLEEP_PIN = 7;
const int MOTOR_DIR_PIN = 8;
const int MOTOR_PWM_PIN = 9;
const int LASER_PIN = 10;
const int ENCODER_A_PIN = 2;
const int ENCODER_B_PIN = 3;
const uint8_t FREE_PINS[] = {11, 12, A0, A1, A2, A3};   // not used by the project: safe to drive
const uint8_t ADC_PINS[] = {A0, A1, A2, A3};

int passCount = 0, failCount = 0, infoCount = 0;

void pass(const __FlashStringHelper *name) {
  Serial.print(F("  [PASS] "));
  Serial.println(name);
  passCount++;
}

void fail(const __FlashStringHelper *name, const __FlashStringHelper *hint) {
  Serial.print(F("  [FAIL] "));
  Serial.print(name);
  Serial.print(F(" - "));
  Serial.println(hint);
  failCount++;
}

void info(const __FlashStringHelper *name) {
  Serial.print(F("  [INFO] "));
  Serial.println(name);
  infoCount++;
}

// ---------- helpers ----------
extern int __heap_start, *__brkval;
int freeRam() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

long readVccMv() {                        // supply voltage from the internal 1.1 V reference
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(5);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {}
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {}
  uint16_t adc = ADC;
  return adc ? 1125300L / adc : 0;
}

volatile uint16_t timerTicks = 0;
ISR(TIMER1_COMPA_vect) {
  timerTicks++;
}

volatile uint16_t encoderCount = 0;
void encoderISR() {
  encoderCount++;
}

const char *pinName(uint8_t p) {
  switch (p) {
    case 11: return "D11";
    case 12: return "D12";
    case A0: return "A0";
    case A1: return "A1";
    case A2: return "A2";
    case A3: return "A3";
    default: return "?";
  }
}

// ---------- tests ----------
void testSignature() {
  uint8_t s0 = boot_signature_byte_get(0);
  uint8_t s1 = boot_signature_byte_get(2);
  uint8_t s2 = boot_signature_byte_get(4);
  Serial.print(F("  chip signature: 0x"));
  Serial.print(s0, HEX);
  Serial.print(F(" 0x"));
  Serial.print(s1, HEX);
  Serial.print(F(" 0x"));
  Serial.print(s2, HEX);
  Serial.print(F("  F_CPU="));
  Serial.print(F_CPU / 1000000UL);
  Serial.println(F(" MHz"));
  if (s0 == 0x1E && s1 == 0x95 && (s2 == 0x0F || s2 == 0x14 || s2 == 0x16)) {
    pass(F("ATmega328 / 328P / 328PB chip, serial link works (you can read this)"));
  } else {
    info(F("signature is not an ATmega328 - is this really a Nano V3?"));
  }
}

void testRam() {
  int fr = freeRam();
  Serial.print(F("  free RAM: "));
  Serial.print(fr);
  Serial.println(F(" bytes of 2048"));
  if (fr > 1200) pass(F("free RAM"));
  else fail(F("free RAM"), F("very low"));
}

void testVcc() {
  long mv = readVccMv();
  Serial.print(F("  supply voltage (ADC vs 1.1 V bandgap): "));
  Serial.print(mv / 1000);
  Serial.print('.');
  Serial.print((mv % 1000) / 10);
  Serial.println(F(" V"));
  if (mv >= 4500 && mv <= 5500) pass(F("supply voltage 4.5 - 5.5 V, ADC converts"));
  else fail(F("supply voltage"), F("outside 4.5 - 5.5 V (USB cable / regulator / ADC problem)"));
}

void testLed() {
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
  info(F("onboard LED D13 blinked 3 times - did you see it? (visual check)"));
}

void testEeprom() {
  int addr = EEPROM.length() - 1;
  uint8_t old = EEPROM.read(addr);
  EEPROM.update(addr, 0xA5);
  bool a = EEPROM.read(addr) == 0xA5;
  EEPROM.update(addr, 0x5A);
  bool b = EEPROM.read(addr) == 0x5A;
  EEPROM.update(addr, old);                       // restore
  if (a && b) pass(F("EEPROM write / read-back"));
  else fail(F("EEPROM"), F("read-back differs"));
}

void testTimer() {
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);   // CTC, prescaler 64
  OCR1A = 249;                                   // 16 MHz / 64 / 250 = 1000 Hz
  TCNT1 = 0;
  timerTicks = 0;
  TIMSK1 = _BV(OCIE1A);
  interrupts();
  unsigned long t0 = millis();
  while (millis() - t0 < 500) {}
  TIMSK1 = 0;
  uint16_t n = timerTicks;
  TCCR1B = _BV(CS11) | _BV(CS10);                // restore Arduino defaults for Timer1
  TCCR1A = _BV(WGM10);
  Serial.print(F("  Timer1 interrupts in 500 ms: "));
  Serial.print(n);
  Serial.println(F(" (expect ~500)"));
  if (n >= 490 && n <= 510) pass(F("timer + interrupt system, 16 MHz clock"));
  else fail(F("timer interrupt"), F("count off - wrong clock (8 MHz?) or interrupts broken"));
}

void testFreePins() {
  bool pullOk = true, outOk = true;
  for (uint8_t i = 0; i < sizeof(FREE_PINS); i++) {
    uint8_t p = FREE_PINS[i];
    pinMode(p, INPUT_PULLUP);
    delay(2);
    if (digitalRead(p) != HIGH) {
      pullOk = false;
      Serial.print(F("    pull-up read LOW on "));
      Serial.println(pinName(p));
    }
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
    delay(1);
    bool h = digitalRead(p) == HIGH;
    digitalWrite(p, LOW);
    delay(1);
    bool l = digitalRead(p) == LOW;
    if (!(h && l)) {
      outOk = false;
      Serial.print(F("    output read-back wrong on "));
      Serial.println(pinName(p));
    }
    pinMode(p, INPUT);
  }
  if (pullOk) pass(F("internal pull-ups on D11 D12 A0-A3 (read HIGH)"));
  else fail(F("pull-ups"), F("a free pin reads LOW: shorted to GND or something is attached"));
  if (outOk) pass(F("digital outputs on D11 D12 A0-A3 (HIGH and LOW read back)"));
  else fail(F("outputs"), F("a pin does not follow what is written"));
}

void testAdc() {
  bool ok = true;
  for (uint8_t i = 0; i < sizeof(ADC_PINS); i++) {
    uint8_t p = ADC_PINS[i];
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
    delay(2);
    int hi = analogRead(p);
    digitalWrite(p, LOW);
    delay(2);
    int lo = analogRead(p);
    pinMode(p, INPUT);
    Serial.print(F("    "));
    Serial.print(pinName(p));
    Serial.print(F(": HIGH -> "));
    Serial.print(hi);
    Serial.print(F("  LOW -> "));
    Serial.println(lo);
    if (hi < 1000 || lo > 24) ok = false;
  }
  if (ok) pass(F("ADC on A0-A3 (about 1023 at HIGH, about 0 at LOW)"));
  else fail(F("ADC"), F("a channel does not read 1023 / 0 (pin loaded or ADC fault)"));
  int a6 = analogRead(A6), a7 = analogRead(A7);
  Serial.print(F("    A6 = "));
  Serial.print(a6);
  Serial.print(F(", A7 = "));
  Serial.print(a7);
  Serial.println(F(" (floating inputs: any value is normal)"));
}

void testPwm() {
  const int duty[3] = {64, 128, 192};
  const long expectUs[3] = {510, 1020, 1530};      // high time at 490 Hz
  bool ok = true;
  pinMode(11, OUTPUT);
  for (int i = 0; i < 3; i++) {
    analogWrite(11, duty[i]);
    delay(10);
    unsigned long hi = pulseIn(11, HIGH, 20000);
    Serial.print(F("    D11 duty "));
    Serial.print(duty[i]);
    Serial.print(F("/255: high time "));
    Serial.print(hi);
    Serial.print(F(" us (expect ~"));
    Serial.print(expectUs[i]);
    Serial.println(F(")"));
    if ((long)hi < expectUs[i] * 8 / 10 || (long)hi > expectUs[i] * 12 / 10) ok = false;
  }
  analogWrite(11, 0);
  pinMode(11, INPUT);
  if (ok) pass(F("PWM on D11 (Timer2) at 25 / 50 / 75 %"));
  else fail(F("PWM"), F("pulse widths wrong (D11 loaded, or Timer2 / clock problem)"));
}

void testI2c() {
  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  delay(2);
  if (digitalRead(A4) == LOW || digitalRead(A5) == LOW) {
    fail(F("I2C lines A4/A5"), F("a line is stuck LOW (short to GND, or a device holding the bus)"));
    return;
  }
  Wire.begin();
  Wire.setClock(100000);
#ifdef WIRE_HAS_TIMEOUT
  Wire.setWireTimeout(3000, true);
#endif
  int found = 0;
  bool mpu = false;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("    I2C device found at 0x"));
      Serial.println(addr, HEX);
      found++;
      if (addr == 0x68) mpu = true;
    }
  }
  pass(F("I2C bus idles HIGH and scans without hanging"));
  if (mpu) {
    Wire.beginTransmission(0x68);
    Wire.write((uint8_t)0x75);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)0x68, (uint8_t)1);
    int who = Wire.available() ? Wire.read() : -1;
    Serial.print(F("    MPU6050 WHO_AM_I = 0x"));
    Serial.println(who, HEX);
    if (who == 0x68) pass(F("MPU6050 answers at 0x68 (WHO_AM_I = 0x68)"));
    else info(F("a device at 0x68 answered but WHO_AM_I is not 0x68 (clone?)"));
  } else if (found == 0) {
    info(F("no I2C device found (MPU6050 not connected - not an error)"));
  } else {
    info(F("I2C devices found, but none at 0x68"));
  }
}

void testJumper() {
  pinMode(12, INPUT);
  pinMode(11, OUTPUT);
  digitalWrite(11, HIGH);
  delay(2);
  bool h = digitalRead(12) == HIGH;
  digitalWrite(11, LOW);
  delay(2);
  bool l = digitalRead(12) == LOW;
  pinMode(11, INPUT);
  if (h && l) pass(F("jumper loop-back D11 -> D12 (both pins work together)"));
  else info(F("optional: join D11 and D12 with a wire to test two pins together"));
}

void testProjectPins() {
  Serial.print(F("  project pins (read only, internal pull-up): "));
  const uint8_t pins[] = {2, 3, 4, 5, 6};
  for (uint8_t i = 0; i < sizeof(pins); i++) {
    pinMode(pins[i], INPUT_PULLUP);
    delay(1);
    Serial.print('D');
    Serial.print(pins[i]);
    Serial.print('=');
    Serial.print(digitalRead(pins[i]) ? F("HIGH ") : F("LOW "));
  }
  Serial.println();
  info(F("D2/D3 encoder and D4/D5/D6 sensors idle HIGH when nothing is seen; LOW = shorted or active"));
}

void testLaser() {
  if (!TEST_LASER_PULSE) {
    info(F("laser test skipped (set TEST_LASER_PULSE = true to pulse D10 for 300 ms)"));
    return;
  }
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, HIGH);
  delay(300);
  digitalWrite(LASER_PIN, LOW);
  info(F("laser on D10 pulsed for 300 ms - did it light? (visual check)"));
}

void testMotor() {
  if (!TEST_MOTOR) {
    info(F("motor test skipped (set TEST_MOTOR = true: the motor WILL turn)"));
    return;
  }
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  encoderCount = 0;
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, RISING);
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);
  for (int dir = 0; dir < 2; dir++) {
    digitalWrite(MOTOR_DIR_PIN, dir ? LOW : HIGH);
    analogWrite(MOTOR_PWM_PIN, MOTOR_TEST_PWM);
    delay(1000);
    analogWrite(MOTOR_PWM_PIN, 0);
    delay(300);
  }
  digitalWrite(MOTOR_SLEEP_PIN, LOW);
  detachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN));
  Serial.print(F("  encoder counts during the motor test: "));
  Serial.println(encoderCount);
  if (encoderCount > 10) pass(F("motor turned and the encoder counted (D2 interrupt works)"));
  else info(F("encoder did not count: motor/encoder not connected, or check D2 wiring (visual check of the motor)"));
}

void testClock() {
  Serial.println(F("  clock check: compare these 5 ticks with a stopwatch (1.00 s apart):"));
  for (int i = 1; i <= 5; i++) {
    delay(1000);
    Serial.print(F("    tick "));
    Serial.print(i);
    Serial.print(F("  (millis = "));
    Serial.print(millis());
    Serial.println(F(")"));
  }
  info(F("clock ticks printed - compare with a stopwatch (a ceramic resonator can be up to 0.5 % off)"));
}

// ---------- run everything ----------
void section(const __FlashStringHelper *name) {
  Serial.println(name);
}

void runAll() {
  passCount = failCount = infoCount = 0;
  Serial.println();
  Serial.println(F("===== NANO V3 SELF-TEST ====="));
  section(F("1 chip / serial")); testSignature();
  section(F("2 RAM")); testRam();
  section(F("3 supply + ADC bandgap")); testVcc();
  section(F("4 onboard LED")); testLed();
  section(F("5 EEPROM")); testEeprom();
  section(F("6 timer interrupt")); testTimer();
  section(F("7 free pins")); testFreePins();
  section(F("8 ADC A0-A3")); testAdc();
  section(F("9 PWM D11")); testPwm();
  section(F("10 I2C A4/A5")); testI2c();
  section(F("11 jumper loop-back")); testJumper();
  section(F("12 project pins")); testProjectPins();
  section(F("13 laser")); testLaser();
  section(F("14 motor")); testMotor();
  section(F("15 clock")); testClock();

  Serial.println();
  Serial.print(F("RESULT: "));
  Serial.print(passCount);
  Serial.print(F(" PASS, "));
  Serial.print(failCount);
  Serial.print(F(" FAIL, "));
  Serial.print(infoCount);
  Serial.println(F(" INFO"));
  if (failCount == 0) {
    Serial.println(F(">>> NANO OK: all tests passed. LED blinks slowly. Send 'r' to run again. <<<"));
  } else {
    Serial.println(F(">>> NANO HAS PROBLEMS: see the [FAIL] lines above. LED blinks fast. Send 'r' to run again. <<<"));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);                                    // let the USB serial settle
  runAll();
}

void loop() {
  unsigned long period = failCount == 0 ? 1000 : 200;
  digitalWrite(LED_PIN, (millis() / period) % 2 ? HIGH : LOW);
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') runAll();
  }
}
