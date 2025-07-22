#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  if (!mpu.begin()) {
    Serial.println("MPU6050 não detectado!");
    while (1);
  }

  // Configura o giroscópio para faixa maior (mais sensível a rotação)
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); // Mais responsivo que 5Hz

  Serial.println("MPU6050 iniciado. Giroscopio em graus/s.");
  delay(1000);
}

void loop() {
  sensors_event_t acc, gyro, temp;
  mpu.getEvent(&acc, &gyro, &temp);

  // Converte de rad/s para graus/s
  float gx = gyro.gyro.x;
  float gy = gyro.gyro.y;
  float gz = gyro.gyro.z;

  Serial.print("Giro X: "); Serial.print(gx, 2);
  Serial.print(" °/s | Y: "); Serial.print(gy, 2);
  Serial.print(" °/s | Z: "); Serial.print(gz, 2);
  Serial.println(" °/s");

  delay(200); // Tempo de leitura mais rápido
}
