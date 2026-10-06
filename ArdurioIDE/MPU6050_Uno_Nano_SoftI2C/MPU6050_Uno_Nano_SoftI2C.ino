/*
  MPU6050 (HW-123 / GY-521 breakout) <-> Arduino Uno / Nano
  Software (bit-banged) I2C wiring:
    VCC -> 5V (or 3.3V if your board is not 5V tolerant)
    GND -> GND
    SDA -> D8
    SCL -> D9
    AD0 -> GND   (sets I2C address to 0x68)
    INT -> not connected (not needed for basic polling)
    XDA/XCL -> not connected

  Uno/Nano (ATmega328P) only expose hardware I2C on A4 (SDA) / A5 (SCL),
  so the Wire library can't be pointed at D8/D9. This sketch bit-bangs
  the I2C protocol on D8/D9 in software instead - no external library
  needed, just this file.

  NOTE: elsewhere in this project D8/D9 are the DRV8874 motor DIR/PWM
  pins (see uno/src/*.cpp and src/*_main.cpp). Wire this sensor up on
  its own board for now, or move the motor pins, before combining this
  with motor-control code on the same Uno/Nano.
*/

#include <Arduino.h>

const uint8_t SDA_PIN = 8;
const uint8_t SCL_PIN = 9;
const uint8_t MPU_ADDR = 0x68;   // 0x69 if AD0 is tied to VCC instead of GND

const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;

// ---- Software I2C (open-drain style: HIGH = release/INPUT_PULLUP, LOW = drive) ----

static void i2cSdaHigh() { pinMode(SDA_PIN, INPUT_PULLUP); }
static void i2cSdaLow()  { pinMode(SDA_PIN, OUTPUT); digitalWrite(SDA_PIN, LOW); }
static void i2cSclHigh() { pinMode(SCL_PIN, INPUT_PULLUP); }
static void i2cSclLow()  { pinMode(SCL_PIN, OUTPUT); digitalWrite(SCL_PIN, LOW); }

static void i2cDelay() { delayMicroseconds(5); } // ~100kHz bus speed

static void i2cInit() {
  i2cSdaHigh();
  i2cSclHigh();
}

static void i2cStart() {
  i2cSdaHigh();
  i2cSclHigh();
  i2cDelay();
  i2cSdaLow();
  i2cDelay();
  i2cSclLow();
  i2cDelay();
}

static void i2cStop() {
  i2cSdaLow();
  i2cDelay();
  i2cSclHigh();
  i2cDelay();
  i2cSdaHigh();
  i2cDelay();
}

// Clocks out one byte MSB-first, returns true if the slave ACKed.
static bool i2cWriteByte(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    if (data & 0x80) i2cSdaHigh(); else i2cSdaLow();
    data <<= 1;
    i2cDelay();
    i2cSclHigh();
    i2cDelay();
    i2cSclLow();
    i2cDelay();
  }

  i2cSdaHigh(); // release SDA so the slave can pull it low for ACK
  i2cDelay();
  i2cSclHigh();
  i2cDelay();
  bool ack = (digitalRead(SDA_PIN) == LOW);
  i2cSclLow();
  i2cDelay();
  return ack;
}

// Clocks in one byte MSB-first, then sends ACK (more bytes wanted) or NACK (last byte).
static uint8_t i2cReadByte(bool ack) {
  uint8_t data = 0;
  i2cSdaHigh(); // release SDA so the slave can drive it
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;
    i2cDelay();
    i2cSclHigh();
    i2cDelay();
    if (digitalRead(SDA_PIN) == HIGH) data |= 1;
    i2cSclLow();
  }

  if (ack) i2cSdaLow(); else i2cSdaHigh();
  i2cDelay();
  i2cSclHigh();
  i2cDelay();
  i2cSclLow();
  i2cSdaHigh();
  return data;
}

static bool mpuWriteReg(uint8_t reg, uint8_t value) {
  i2cStart();
  bool ok = i2cWriteByte(MPU_ADDR << 1); // address + write bit
  ok &= i2cWriteByte(reg);
  ok &= i2cWriteByte(value);
  i2cStop();
  return ok;
}

static bool mpuReadBytes(uint8_t startReg, uint8_t *buf, uint8_t len) {
  i2cStart();
  bool ok = i2cWriteByte(MPU_ADDR << 1); // address + write bit
  ok &= i2cWriteByte(startReg);
  i2cStart();                             // repeated start
  ok &= i2cWriteByte((MPU_ADDR << 1) | 1); // address + read bit
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = i2cReadByte(i < len - 1);     // ACK all but the last byte
  }
  i2cStop();
  return ok;
}

void setup() {
  Serial.begin(9600);
  i2cInit();

  // Wake up the MPU6050 (it starts in sleep mode by default)
  if (mpuWriteReg(REG_PWR_MGMT_1, 0x00)) {
    Serial.println("MPU6050 initialized.");
  } else {
    Serial.println("MPU6050 not responding - check wiring (SDA=D8, SCL=D9).");
  }
}

void loop() {
  // Read 14 bytes starting at ACCEL_XOUT_H:
  // Accel X,Y,Z (6 bytes) + Temp (2 bytes) + Gyro X,Y,Z (6 bytes)
  uint8_t raw[14];

  if (mpuReadBytes(REG_ACCEL_XOUT_H, raw, 14)) {
    int16_t ax = (raw[0] << 8) | raw[1];
    int16_t ay = (raw[2] << 8) | raw[3];
    int16_t az = (raw[4] << 8) | raw[5];
    int16_t rawTemp = (raw[6] << 8) | raw[7];
    int16_t gx = (raw[8] << 8) | raw[9];
    int16_t gy = (raw[10] << 8) | raw[11];
    int16_t gz = (raw[12] << 8) | raw[13];

    // Default accel range is +/-2g -> divide by 16384 to get g's
    float accelX = ax / 16384.0;
    float accelY = ay / 16384.0;
    float accelZ = az / 16384.0;

    // Default gyro range is +/-250 deg/s -> divide by 131 to get deg/s
    float gyroX = gx / 131.0;
    float gyroY = gy / 131.0;
    float gyroZ = gz / 131.0;

    float tempC = rawTemp / 340.0 + 36.53;

    Serial.print("Accel(g) X: "); Serial.print(accelX, 3);
    Serial.print(" Y: "); Serial.print(accelY, 3);
    Serial.print(" Z: "); Serial.print(accelZ, 3);

    Serial.print("   Gyro(deg/s) X: "); Serial.print(gyroX, 2);
    Serial.print(" Y: "); Serial.print(gyroY, 2);
    Serial.print(" Z: "); Serial.print(gyroZ, 2);

    Serial.print("   Temp: "); Serial.print(tempC, 1);
    Serial.println(" C");
  } else {
    Serial.println("Error: I2C read failed (no ACK from MPU6050)");
  }

  delay(500); // slow it down so the serial monitor is readable
}
