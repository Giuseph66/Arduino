#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define SDA_PIN 23
#define SCL_PIN 19

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 400000); // I²C a 400 kHz

  // Scanner simples
  byte found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("I2C encontrado em 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println("Nada no I2C.");

  // Inicializa MPU6050 no 0x68
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("MPU6050 não encontrado!");
    while (1) delay(10);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  Serial.printf("Acc [m/s^2]: %.2f %.2f %.2f | Gyro [°/s]: %.2f %.2f %.2f\n",
                a.acceleration.x, a.acceleration.y, a.acceleration.z,
                g.gyro.x, g.gyro.y, g.gyro.z);
  delay(100);
}
