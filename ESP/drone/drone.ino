#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Saída: uma linha JSON por amostra, ~50 Hz.
// {"q":[w,x,y,z],"r":roll,"p":pitch,"y":yaw,"ax":..,"ay":..,"az":..,"gx":..,"gy":..,"gz":..,"t":temp}
// Orientação estimada por filtro Mahony (quaternion, sem gimbal lock).
// Comandos pela serial: 'c' recalibra gyro (manter parado), 'r' reinicia orientação.

Adafruit_MPU6050 mpu;

const float RAD2DEG = 180.0 / PI;
const float KP = 1.5;                  // ganho de correção pelo acelerômetro
const float KI = 0.0;                  // gyro já tem offset calibrado
const unsigned long PERIOD_US = 20000; // 50 Hz

float q0 = 1, q1 = 0, q2 = 0, q3 = 0;  // sensor -> mundo
float eInt[3] = {0, 0, 0};
float gxOff = 0, gyOff = 0, gzOff = 0;
unsigned long lastUs = 0;

void calibrarGyro() {
  const int N = 500;
  double sx = 0, sy = 0, sz = 0;
  sensors_event_t a, g, t;
  for (int i = 0; i < N; i++) {
    mpu.getEvent(&a, &g, &t);
    sx += g.gyro.x;
    sy += g.gyro.y;
    sz += g.gyro.z;
    delay(2);
  }
  gxOff = sx / N;
  gyOff = sy / N;
  gzOff = sz / N;
}

// Inicializa quaternion alinhando eixo Z do sensor com a gravidade medida
void inicializarOrientacao() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);
  float ax = a.acceleration.x, ay = a.acceleration.y, az = a.acceleration.z;
  float n = sqrt(ax * ax + ay * ay + az * az);
  if (n < 1e-3) { q0 = 1; q1 = q2 = q3 = 0; return; }
  ax /= n; ay /= n; az /= n;
  // rotação mínima que leva (ax,ay,az) para (0,0,1)
  float d = az;
  float cx = ay, cy = -ax, cz = 0;      // (a) x (0,0,1)
  q0 = 1 + d; q1 = cx; q2 = cy; q3 = cz;
  float qn = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  if (qn < 1e-6) { q0 = 0; q1 = 1; q2 = 0; q3 = 0; return; } // 180°
  q0 /= qn; q1 /= qn; q2 /= qn; q3 /= qn;
  eInt[0] = eInt[1] = eInt[2] = 0;
}

// Mahony AHRS (IMU, sem magnetômetro). gx,gy,gz em rad/s.
void mahonyUpdate(float gx, float gy, float gz, float ax, float ay, float az, float dt) {
  float n = sqrt(ax * ax + ay * ay + az * az);
  if (n > 1e-3) {
    ax /= n; ay /= n; az /= n;
    // gravidade estimada no frame do sensor
    float vx = 2 * (q1 * q3 - q0 * q2);
    float vy = 2 * (q0 * q1 + q2 * q3);
    float vz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;
    // erro = medido x estimado
    float ex = ay * vz - az * vy;
    float ey = az * vx - ax * vz;
    float ez = ax * vy - ay * vx;
    if (KI > 0) {
      eInt[0] += ex * dt; eInt[1] += ey * dt; eInt[2] += ez * dt;
      gx += KI * eInt[0]; gy += KI * eInt[1]; gz += KI * eInt[2];
    }
    gx += KP * ex; gy += KP * ey; gz += KP * ez;
  }
  // integra taxa de mudança do quaternion
  float hdt = 0.5f * dt;
  float dq0 = (-q1 * gx - q2 * gy - q3 * gz) * hdt;
  float dq1 = ( q0 * gx + q2 * gz - q3 * gy) * hdt;
  float dq2 = ( q0 * gy - q1 * gz + q3 * gx) * hdt;
  float dq3 = ( q0 * gz + q1 * gy - q2 * gx) * hdt;
  q0 += dq0; q1 += dq1; q2 += dq2; q3 += dq3;
  float qn = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  q0 /= qn; q1 /= qn; q2 /= qn; q3 /= qn;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  Wire.setClock(400000);

  if (!mpu.begin()) {
    while (1) {
      Serial.println("{\"err\":\"MPU6050 nao encontrado\"}");
      delay(1000);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
  delay(100);

  calibrarGyro();
  inicializarOrientacao();
  lastUs = micros();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c') calibrarGyro();
    if (c == 'r') inicializarOrientacao();
  }

  unsigned long now = micros();
  if (now - lastUs < PERIOD_US) return;
  float dt = (now - lastUs) / 1e6;
  lastUs = now;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float gxr = g.gyro.x - gxOff;
  float gyr = g.gyro.y - gyOff;
  float gzr = g.gyro.z - gzOff;

  mahonyUpdate(gxr, gyr, gzr, ax, ay, az, dt);

  // Euler (ZYX) só para leitura humana; painel usa o quaternion
  float roll  = atan2(2 * (q0 * q1 + q2 * q3), 1 - 2 * (q1 * q1 + q2 * q2)) * RAD2DEG;
  float sinp  = 2 * (q0 * q2 - q3 * q1);
  float pitch = (fabs(sinp) >= 1 ? copysign(90.0f, sinp) : asin(sinp) * RAD2DEG);
  float yaw   = atan2(2 * (q0 * q3 + q1 * q2), 1 - 2 * (q2 * q2 + q3 * q3)) * RAD2DEG;

  Serial.print("{\"q\":["); Serial.print(q0, 4);
  Serial.print(","); Serial.print(q1, 4);
  Serial.print(","); Serial.print(q2, 4);
  Serial.print(","); Serial.print(q3, 4);
  Serial.print("],\"r\":"); Serial.print(roll, 2);
  Serial.print(",\"p\":"); Serial.print(pitch, 2);
  Serial.print(",\"y\":"); Serial.print(yaw, 2);
  Serial.print(",\"ax\":"); Serial.print(ax, 2);
  Serial.print(",\"ay\":"); Serial.print(ay, 2);
  Serial.print(",\"az\":"); Serial.print(az, 2);
  Serial.print(",\"gx\":"); Serial.print(gxr * RAD2DEG, 1);
  Serial.print(",\"gy\":"); Serial.print(gyr * RAD2DEG, 1);
  Serial.print(",\"gz\":"); Serial.print(gzr * RAD2DEG, 1);
  Serial.print(",\"t\":"); Serial.print(temp.temperature, 1);
  Serial.println("}");
}
