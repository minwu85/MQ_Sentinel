/*
  MPU6050 (HW-123 / GY-521 breakout) <-> Arduino Mega
  I2C wiring:
    VCC -> 5V (or 3.3V if your board is not 5V tolerant)
    GND -> GND
    SCL -> Mega pin 21
    SDA -> Mega pin 20
    AD0 -> GND   (sets I2C address to 0x68)
    INT -> not connected (not needed for basic polling)
    XDA/XCL -> not connected

  Uses raw register access via Wire, no external library needed.
*/

#include <Wire.h>

const int MPU_ADDR = 0x68;   // 0x69 if AD0 is tied to VCC instead of GND

// Register addresses
const byte REG_PWR_MGMT_1 = 0x6B;
const byte REG_ACCEL_XOUT_H = 0x3B;

void setup() {
  Serial.begin(9600);
  Wire.begin();  // Mega uses pins 20 (SDA) and 21 (SCL) automatically

  // Wake up the MPU6050 (it starts in sleep mode by default)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();

  Serial.println("MPU6050 initialized.");
}

void loop() {
  // Read 14 bytes starting at ACCEL_XOUT_H:
  // Accel X,Y,Z (6 bytes) + Temp (2 bytes) + Gyro X,Y,Z (6 bytes)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  if (Wire.available() == 14) {
    int16_t ax = Wire.read() << 8 | Wire.read();
    int16_t ay = Wire.read() << 8 | Wire.read();
    int16_t az = Wire.read() << 8 | Wire.read();
    int16_t rawTemp = Wire.read() << 8 | Wire.read();
    int16_t gx = Wire.read() << 8 | Wire.read();
    int16_t gy = Wire.read() << 8 | Wire.read();
    int16_t gz = Wire.read() << 8 | Wire.read();

    // Convert to real units
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
    Serial.println("Error: could not read 14 bytes from MPU6050");
  }

  delay(500);  // slow it down so the serial monitor is readable
}
