// ==== OLED SSD1306 128x64 + MPU6050 (Anime Face V2 - filtro complementar) ====
// - Olhos + sobrancelhas + boca estilo anime
// - Emoções mudam conforme pitch/roll + rotação rápida (giroscópio)
// - Filtro complementar p/ ângulos estáveis + responsivos
// - Compatível com Arduino Nano (A4/A5) e ESP32 (21/22)

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MPU6050 mpu;

// ---------- ajustes rápidos ----------
#define OLED_ADDR   0x3C
#define USE_ROTATION_180   0    // 0 normal, 1 vira a tela
#define I2C_400KHZ         1    // 1 = 400kHz, 0 = 100kHz

// limiares de emoção (em graus ou °/s)
static const float TH_PITCH_HAPPY =  12.0f;  // inclina pra frente (positivo) -> feliz
static const float TH_PITCH_SAD   = -12.0f;  // inclina pra trás (negativo) -> triste
static const float TH_ROLL_ANGRY  =  20.0f;  // roll alto e pitch quase zero -> bravo
static const float TH_GYRO_SURP   = 220.0f;  // °/s -> surpresa/piscada

// filtro
static const float LPF_ALPHA = 0.15f;   // passa-baixas p/ sinais crús
static const float CF_ALPHA  = 0.96f;   // filtro complementar (0.92–0.98)

// estados
enum Mood { HAPPY, NEUTRAL, SAD, SURPRISED, ANGRY };

// offsets e filtros

// offsets e filtros
float ax_off=0, ay_off=0, az_off=0;
float gx_off=0, gy_off=0, gz_off=0;
float ax_f=0, ay_f=0, az_f=0;
float gx_f=0, gy_f=0, gz_f=0;

// ângulos finais (filtro complementar)
float pitch_cf=0.0f;  // graus
float roll_cf =0.0f;  // graus

// piscada
unsigned long nextBlink = 0;
unsigned long blinkUntil = 0;

// tempo
unsigned long lastMicros = 0;

void thickLine(int x0,int y0,int x1,int y1,int t=3) {
  for (int i=-t/2; i<=t/2; ++i) display.drawLine(x0, y0+i, x1, y1+i, SSD1306_WHITE);
}

void drawEye(int cx, int cy, int r, int offx, int offy, bool closed) {
  if (closed) { thickLine(cx-r, cy, cx+r, cy, 2); return; }
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  offx = constrain(offx, -r/2, r/2);
  offy = constrain(offy, -r/2, r/2);
  display.fillCircle(cx + offx, cy + offy, r/3, SSD1306_BLACK);
}

void drawBrowsAnime(int cxL, int cyL, int cxR, int cyR, Mood mood, float rollDeg) {
  int yBase = cyL - 12, span = 12;
  int l_y1, l_y2, r_y1, r_y2;

  switch (mood) {
    case HAPPY:     l_y1=yBase-2; l_y2=yBase+3; r_y1=yBase+3; r_y2=yBase-2; break;
    case SAD:       l_y1=yBase-2; l_y2=yBase+4; r_y1=yBase+4; r_y2=yBase-2; break;
    case ANGRY:     l_y1=yBase+4; l_y2=yBase-3; r_y1=yBase-3; r_y2=yBase+4; break;
    case SURPRISED: l_y1=yBase-3; l_y2=yBase-3; r_y1=yBase-3; r_y2=yBase-3; break;
    default:        l_y1=yBase+1; l_y2=yBase;   r_y1=yBase;   r_y2=yBase+1; break;
  }

  int tilt = (int)(rollDeg/10.0f);
  l_y1 += tilt; l_y2 += tilt; r_y1 += tilt; r_y2 += tilt;

  thickLine(cxL - span, l_y1, cxL + span, l_y2, 3);
  thickLine(cxR - span, r_y1, cxR + span, r_y2, 3);
}

void drawMouth(int cx, int cy, Mood mood) {
  switch (mood) {
    case HAPPY: for (int a=20; a<=160; a+=2){float r=a*PI/180.0f; display.drawPixel(cx+cos(r)*18, cy+sin(r)*10, SSD1306_WHITE);} break;
    case SAD:   for (int a=200;a<=340; a+=2){float r=a*PI/180.0f; display.drawPixel(cx+cos(r)*18, cy+sin(r)*10, SSD1306_WHITE);} break;
    case SURPRISED: display.drawCircle(cx, cy+2, 7, SSD1306_WHITE); break;
    case ANGRY:     display.drawFastHLine(cx-10, cy+6, 20, SSD1306_WHITE); break;
    default:        display.drawFastHLine(cx-12, cy+6, 24, SSD1306_WHITE); break;
  }
}

void drawFace(float pitchDeg, float rollDeg, float gyroRate) {
  display.clearDisplay();

  const int eyeR=12, leftX=44, rightX=84, eyeY=32, mouthX=64, mouthY=46;

  int px = constrain((int)(rollDeg/8.0f),  -4, 4);
  int py = constrain((int)(-pitchDeg/8.0f), -3, 3);

  bool blink = (millis() < blinkUntil);

  Mood mood = NEUTRAL;
  if (gyroRate > TH_GYRO_SURP)                     mood = SURPRISED;
  else if (fabs(pitchDeg) < 10 && fabs(rollDeg) > TH_ROLL_ANGRY) mood = ANGRY;
  else if (pitchDeg > TH_PITCH_HAPPY)              mood = HAPPY;
  else if (pitchDeg < TH_PITCH_SAD)                mood = SAD;

  drawEye(leftX,  eyeY, eyeR, px, py, blink);
  drawEye(rightX, eyeY, eyeR, px, py, blink);
  drawBrowsAnime(leftX, eyeY, rightX, eyeY, mood, rollDeg);
  drawMouth(mouthX, mouthY, mood);

  display.display();
}

void calibrate(int n=250) {
  long ax=0, ay=0, az=0, gx=0, gy=0, gz=0;
  for (int i=0;i<n;i++) {
    int16_t axr, ayr, azr, gxr, gyr, gzr;
    mpu.getMotion6(&axr,&ayr,&azr,&gxr,&gyr,&gzr);
    ax+=axr; ay+=ayr; az+=azr; gx+=gxr; gy+=gyr; gz+=gzr;
    delay(2);
  }
  ax_off=(float)ax/n; ay_off=(float)ay/n; az_off=(float)az/n;
  gx_off=(float)gx/n; gy_off=(float)gy/n; gz_off=(float)gz/n;
}

void setup() {
#if defined(ESP32)
  Wire.begin(21,22);
#else
  Wire.begin(); // Nano: SDA=A4, SCL=A5
#endif
#if I2C_400KHZ
  Wire.setClock(400000);
#endif

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Erro no display"); while(1);
  }
#if USE_ROTATION_180
  display.setRotation(2);
#endif
  display.clearDisplay(); display.display();

  mpu.initialize();
  if (!mpu.testConnection()) { Serial.println("Erro no MPU6050!"); while(1); }

  calibrate();
  nextBlink = millis() + 1200 + random(800);
  lastMicros = micros();
}

void loop() {
  // tempo delta
  unsigned long nowMicros = micros();
  float dt = (nowMicros - lastMicros) / 1e6f;
  lastMicros = nowMicros;

  // lê sensores
  int16_t axr, ayr, azr, gxr, gyr, gzr;
  mpu.getMotion6(&axr,&ayr,&azr,&gxr,&gyr,&gzr);

  // remove offset e normaliza (a: g; g: °/s)
  float ax = (axr-ax_off)/16384.0f;
  float ay = (ayr-ay_off)/16384.0f;
  float az = (azr-az_off)/16384.0f;
  float gx = (gxr-gx_off)/131.0f;
  float gy = (gyr-gy_off)/131.0f;
  float gz = (gzr-gz_off)/131.0f;

  // filtros low-pass
  ax_f += LPF_ALPHA*(ax-ax_f);
  ay_f += LPF_ALPHA*(ay-ay_f);
  az_f += LPF_ALPHA*(az-az_f);
  gx_f += LPF_ALPHA*(gx-gx_f);
  gy_f += LPF_ALPHA*(gy-gy_f);
  gz_f += LPF_ALPHA*(gz-gz_f);

  // ângulos por acelerômetro (estáveis)
  float pitchAcc = atan2f(-ax_f, sqrtf(ay_f*ay_f + az_f*az_f)) * 180.0f/PI;
  float rollAcc  = atan2f( ay_f,  az_f) * 180.0f/PI;

  // integração do giroscópio + correção (filtro complementar)
  pitch_cf = CF_ALPHA*(pitch_cf + gx_f*dt) + (1.0f-CF_ALPHA)*pitchAcc;
  roll_cf  = CF_ALPHA*(roll_cf  + gy_f*dt) + (1.0f-CF_ALPHA)*rollAcc;

  // taxa de rotação total (para surpresa/piscada rápida)
  float gyroRate = sqrtf(gx_f*gx_f + gy_f*gy_f + gz_f*gz_f);

  // piscadas: aleatórias + por movimento rápido
  unsigned long now = millis();
  if (now > nextBlink) {
    blinkUntil = now + 120;
    nextBlink = now + 1200 + random(1200);
  }
  if (gyroRate > (TH_GYRO_SURP+40.0f)) blinkUntil = now + 80;

  drawFace(pitch_cf, roll_cf, gyroRate);
  delay(16); // ~60 FPS
}
