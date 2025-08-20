#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <U8g2lib.h>
#include <math.h>

// I2C ESP32
#define I2C_SDA 21
#define I2C_SCL 22

// OLED 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA
);

Adafruit_MPU6050 mpu;

// ---------- Parâmetros de desenho ----------
const float HALF_SIDE = 10.0f;   // meia aresta do cubo (maior = menos "denteado")
const float FOCAL     = 80.0f;   // distância focal (reduzida para perspectiva melhor)
const float Z_OFFSET  = 70.0f;   // afasta o cubo (reduzido para parecer mais próximo)
const int   CX = 64;             // centro X
const int   CY = 40;             // centro Y (ajustado para parecer apoiado)

// ---------- Filtro/estado ----------
float roll  = 0.0f, pitch = 0.0f, yaw = 0.0f;   // rad
float roll_vis = 0.0f, pitch_vis = 0.0f, yaw_vis = 0.0f; // ângulos suavizados p/ desenho
const float ALPHA = 0.97f;       // peso do gyro no complementar (aumentado para mais estabilidade)
const float SMOOTH = 0.90f;      // suavização de exibição (aumentada para menos tremor)

uint32_t lastMicros = 0;
const uint32_t UPDATE_MS = 50;   // ~20 FPS (mais lento = menos tremor)
uint32_t lastUpdate = 0;

// ---------- Calibração de bias (gyro) ----------
float gx_bias = 0.0f, gy_bias = 0.0f, gz_bias = 0.0f;

// ---------- Detecção de repouso ----------
bool   stationary = false;
int    still_frames = 0, move_frames = 0;
const  int   STILL_FRAMES_TRG = 10;   // ~ 10*50ms = 500ms parado ativa "freeze"
const  int   MOVE_FRAMES_TRG  = 2;    // ~ 100ms em movimento sai do "freeze"
const  float G = 9.80665f;
const  float GYRO_THRESH_DEG = 1.2f;  // deg/s (mais sensível)
const  float ACC_NORM_THRESH = 0.08f; // tolerância de 0.08g ao redor de 1g (mais sensível)

// ---------- Persistência das setas ----------
bool arrow_x_active = false, arrow_y_active = false, arrow_z_active = false;
int arrow_x_timer = 0, arrow_y_timer = 0, arrow_z_timer = 0;
const int ARROW_PERSISTENCE = 20; // frames que a seta fica visível (20*50ms = 1000ms)

// ---------- Cubo ----------
struct Vec3 { float x, y, z; };
const Vec3 CUBE[8] = {
  {-HALF_SIDE, -HALF_SIDE, -HALF_SIDE},
  { HALF_SIDE, -HALF_SIDE, -HALF_SIDE},
  { HALF_SIDE,  HALF_SIDE, -HALF_SIDE},
  {-HALF_SIDE,  HALF_SIDE, -HALF_SIDE},
  {-HALF_SIDE, -HALF_SIDE,  HALF_SIDE},
  { HALF_SIDE, -HALF_SIDE,  HALF_SIDE},
  { HALF_SIDE,  HALF_SIDE,  HALF_SIDE},
  {-HALF_SIDE,  HALF_SIDE,  HALF_SIDE}
};
const uint8_t EDGES[12][2] = {
  {0,1},{1,2},{2,3},{3,0},
  {4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

static inline Vec3 rotateXYZ(const Vec3& v, float rX, float rY, float rZ) {
  // Euler Z-Y-X
  float cr = cosf(rX), sr = sinf(rX);
  float cp = cosf(rY), sp = sinf(rY);
  float cy = cosf(rZ), sy = sinf(rZ);

  // Rx
  float x1 = v.x;
  float y1 = cr*v.y - sr*v.z;
  float z1 = sr*v.y + cr*v.z;
  // Ry
  float x2 =  cp*x1 + sp*z1;
  float y2 =  y1;
  float z2 = -sp*x1 + cp*z1;
  // Rz
  Vec3 out;
  out.x = cy*x2 - sy*y2;
  out.y = sy*x2 + cy*y2;
  out.z = z2;
  return out;
}

static inline void projectTo2D(const Vec3& v3, int& x2d, int& y2d) {
  float z = v3.z + Z_OFFSET;
  if (z < 1.0f) z = 1.0f;
  float px = (v3.x * FOCAL) / z;
  float py = (v3.y * FOCAL) / z;
  x2d = CX + (int)lroundf(px);
  y2d = CY - (int)lroundf(py);
}

// Linhas mais “grossas”: escolhe direção do espessamento conforme a inclinação
void drawThickLine(int x0,int y0,int x1,int y1) {
  u8g2.drawLine(x0,y0,x1,y1);
  int dx = abs(x1-x0), dy = abs(y1-y0);
  if (dx >= dy) {
    u8g2.drawLine(x0, y0+1, x1, y1+1);
    // opcional: deixe a próxima linha comentada se achar grosso demais
    // u8g2.drawLine(x0, y0-1, x1, y1-1);
  } else {
    u8g2.drawLine(x0+1, y0, x1+1, y1);
    // u8g2.drawLine(x0-1, y0, x1-1, y1);
  }
}

void drawCube(float rX, float rY, float rZ) {
  int X[8], Y[8];
  for (int i = 0; i < 8; i++) {
    Vec3 vr = rotateXYZ(CUBE[i], rX, rY, rZ);
    projectTo2D(vr, X[i], Y[i]);
  }
  for (int e = 0; e < 12; e++) {
    uint8_t a = EDGES[e][0], b = EDGES[e][1];
    drawThickLine(X[a], Y[a], X[b], Y[b]);
  }
}

// Função para desenhar setas de direção com persistência
void drawDirectionArrows(float gx, float gy, float gz) {
  const float ARROW_THRESHOLD = 3.0f; // deg/s - limiar aumentado para mais estabilidade
  const float RAD2DEG = 57.2957795f;
  
  // Converte para graus/s
  float gx_deg = gx * RAD2DEG;
  float gy_deg = gy * RAD2DEG;
  float gz_deg = gz * RAD2DEG;
  
  // Atualiza estado das setas X
  if (fabsf(gx_deg) > ARROW_THRESHOLD) {
    arrow_x_active = true;
    arrow_x_timer = ARROW_PERSISTENCE;
  } else {
    if (arrow_x_timer > 0) arrow_x_timer--;
    if (arrow_x_timer == 0) arrow_x_active = false;
  }
  
  // Atualiza estado das setas Y
  if (fabsf(gy_deg) > ARROW_THRESHOLD) {
    arrow_y_active = true;
    arrow_y_timer = ARROW_PERSISTENCE;
  } else {
    if (arrow_y_timer > 0) arrow_y_timer--;
    if (arrow_y_timer == 0) arrow_y_active = false;
  }
  
  // Atualiza estado das setas Z
  if (fabsf(gz_deg) > ARROW_THRESHOLD) {
    arrow_z_active = true;
    arrow_z_timer = ARROW_PERSISTENCE;
  } else {
    if (arrow_z_timer > 0) arrow_z_timer--;
    if (arrow_z_timer == 0) arrow_z_active = false;
  }
  
  // Posições das setas (ao redor do cubo)
  int arrow_x = 64; // centro X
  int arrow_y = 40; // centro Y
  
  // Seta X (Roll) - esquerda/direita
  if (arrow_x_active) {
    if (gx_deg > 0) {
      // Seta para direita
      u8g2.drawLine(arrow_x+15, arrow_y, arrow_x+25, arrow_y);
      u8g2.drawLine(arrow_x+20, arrow_y-3, arrow_x+25, arrow_y);
      u8g2.drawLine(arrow_x+20, arrow_y+3, arrow_x+25, arrow_y);
    } else {
      // Seta para esquerda
      u8g2.drawLine(arrow_x-15, arrow_y, arrow_x-25, arrow_y);
      u8g2.drawLine(arrow_x-20, arrow_y-3, arrow_x-25, arrow_y);
      u8g2.drawLine(arrow_x-20, arrow_y+3, arrow_x-25, arrow_y);
    }
  }
  
  // Seta Y (Pitch) - cima/baixo
  if (arrow_y_active) {
    if (gy_deg > 0) {
      // Seta para baixo
      u8g2.drawLine(arrow_x, arrow_y+15, arrow_x, arrow_y+25);
      u8g2.drawLine(arrow_x-3, arrow_y+20, arrow_x, arrow_y+25);
      u8g2.drawLine(arrow_x+3, arrow_y+20, arrow_x, arrow_y+25);
    } else {
      // Seta para cima
      u8g2.drawLine(arrow_x, arrow_y-15, arrow_x, arrow_y-25);
      u8g2.drawLine(arrow_x-3, arrow_y-20, arrow_x, arrow_y-25);
      u8g2.drawLine(arrow_x+3, arrow_y-20, arrow_x, arrow_y-25);
    }
  }
  
  // Seta Z (Yaw) - rotação (círculo pequeno)
  if (arrow_z_active) {
    // Desenha um pequeno círculo com seta
    u8g2.drawCircle(arrow_x+35, arrow_y-15, 5);
    if (gz_deg > 0) {
      // Seta horária
      u8g2.drawLine(arrow_x+35, arrow_y-20, arrow_x+35, arrow_y-15);
      u8g2.drawLine(arrow_x+35, arrow_y-15, arrow_x+38, arrow_y-15);
    } else {
      // Seta anti-horária
      u8g2.drawLine(arrow_x+35, arrow_y-20, arrow_x+35, arrow_y-15);
      u8g2.drawLine(arrow_x+35, arrow_y-15, arrow_x+32, arrow_y-15);
    }
  }
}

// Calibra bias do giroscópio (sensor em repouso!)
void calibrateGyroBias(uint16_t ms = 2000) {
  const uint16_t dt_ms = 5;
  uint32_t reps = ms / dt_ms;
  double sx=0, sy=0, sz=0;
  sensors_event_t a, g, t;

  for (uint32_t i=0;i<reps;i++) {
    mpu.getEvent(&a,&g,&t);
    sx += g.gyro.x;
    sy += g.gyro.y;
    sz += g.gyro.z;
    delay(dt_ms);
  }
  gx_bias = (float)(sx / reps);
  gy_bias = (float)(sy / reps);
  gz_bias = (float)(sz / reps);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "Inicializando MPU6050...");
  u8g2.sendBuffer();

  if (!mpu.begin(0x68, &Wire)) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "ERRO: MPU6050 (0x68)");
    u8g2.drawStr(0, 28, "Cheque conexoes.");
    u8g2.sendBuffer();
    while (true) { delay(1000); }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);  // Mais sensível que 500_DEG
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ); // Filtro mais rápido
  delay(100);

  // Calibrar gyro (fique parado!)
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "Calibrando gyro (2s)...");
  u8g2.sendBuffer();
  calibrateGyroBias(2000);

  // Estado inicial pelos eixos da gravidade
  sensors_event_t a, g, t;
  mpu.getEvent(&a,&g,&t);
  roll  = atan2f(a.acceleration.y, a.acceleration.z);
  pitch = atan2f(-a.acceleration.x, sqrtf(a.acceleration.y*a.acceleration.y + a.acceleration.z*a.acceleration.z));
  yaw   = 0.0f;

  roll_vis = roll; pitch_vis = pitch; yaw_vis = yaw;

  lastMicros = micros();

  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "Pronto. Movimente o sensor.");
  u8g2.sendBuffer();
  delay(600);
}

void loop() {
  uint32_t now = millis();
  if (now - lastUpdate < UPDATE_MS) return;
  lastUpdate = now;

  // dt para integrar gyro
  uint32_t mu = micros();
  float dt = (mu - lastMicros) / 1e6f;
  lastMicros = mu;
  if (dt <= 0.0f || dt > 0.5f) dt = UPDATE_MS / 1000.0f;

  sensors_event_t a, g, t;
  mpu.getEvent(&a,&g,&t);

  // Tira bias do gyro
  float gx = g.gyro.x - gx_bias; // rad/s
  float gy = g.gyro.y - gy_bias; // rad/s
  float gz = g.gyro.z - gz_bias; // rad/s

  // Normas para detecção de repouso
  float acc_norm = sqrtf(a.acceleration.x*a.acceleration.x +
                         a.acceleration.y*a.acceleration.y +
                         a.acceleration.z*a.acceleration.z);

  const float RAD2DEG = 57.2957795f;
  float gyro_abs_deg = fmaxf(fmaxf(fabsf(gx), fabsf(gy)), fabsf(gz)) * RAD2DEG;
  bool gyroQuiet  = (gyro_abs_deg < GYRO_THRESH_DEG);
  bool accelQuiet = (fabsf(acc_norm - G) < (ACC_NORM_THRESH * G));
  bool quiet      = gyroQuiet && accelQuiet;

  if (quiet) { still_frames++; move_frames = 0; }
  else       { move_frames++;  still_frames = 0; }

  if (!stationary && still_frames >= STILL_FRAMES_TRG) stationary = true;
  if ( stationary && move_frames  >= MOVE_FRAMES_TRG ) stationary = false;

  // Filtro complementar apenas quando em movimento
  if (!stationary) {
    float roll_acc  = atan2f(a.acceleration.y, a.acceleration.z);
    float pitch_acc = atan2f(-a.acceleration.x, sqrtf(a.acceleration.y*a.acceleration.y + a.acceleration.z*a.acceleration.z));

    roll  = ALPHA * (roll  + gx * dt) + (1.0f - ALPHA) * roll_acc;
    pitch = ALPHA * (pitch + gy * dt) + (1.0f - ALPHA) * pitch_acc;
    yaw  += gz * dt; // sem magnetometro -> pode derivar durante movimento
  } else {
    // Em repouso: congele orientação exibida e zera yaw gradualmente
    // (opcional) ajuste fino do bias enquanto parado:
    gx_bias = 0.999f*gx_bias + 0.001f*(g.gyro.x); // adapta muito devagar
    gy_bias = 0.999f*gy_bias + 0.001f*(g.gyro.y);
    gz_bias = 0.999f*gz_bias + 0.001f*(g.gyro.z);
    
    // Zera o yaw gradualmente quando em repouso
    yaw *= 0.95f; // Decai para zero
  }

  // Suavização para exibição (tira tremor residual)
  if (stationary) {
    // Suavização extra quando parado para eliminar tremor
    roll_vis  = 0.98f*roll_vis  + 0.02f*roll;
    pitch_vis = 0.98f*pitch_vis + 0.02f*pitch;
    yaw_vis   = 0.98f*yaw_vis   + 0.02f*yaw;
  } else {
    // Suavização normal quando em movimento
    roll_vis  = SMOOTH*roll_vis  + (1.0f - SMOOTH)*roll;
    pitch_vis = SMOOTH*pitch_vis + (1.0f - SMOOTH)*pitch;
    yaw_vis   = SMOOTH*yaw_vis   + (1.0f - SMOOTH)*yaw;
  }

  // Desenho
  u8g2.clearBuffer();
  drawCube(roll_vis, pitch_vis, yaw_vis);
  drawDirectionArrows(gx, gy, gz); // Desenha setas de direção

  // HUD reorganizado - valores nas laterais do cubo
  u8g2.setFont(u8g2_font_5x7_tf);
  
  // Status no topo
  u8g2.setCursor(0, 8);
  u8g2.print(stationary ? "PARADO" : "MOV.");
  
  // Explicação das setas
  u8g2.setCursor(70, 8);
  u8g2.print("SETAS");
  
  // Indicador de setas ativas
  if (arrow_x_active || arrow_y_active || arrow_z_active) {
    u8g2.setCursor(110, 8);
    u8g2.print("ON");
  }
  
  // Labels para identificar os valores
  u8g2.setCursor(0, 16);
  u8g2.print("ACC");
  u8g2.setCursor(90, 16);
  u8g2.print("GYR");
  
  // Ângulos na parte inferior
  u8g2.setCursor(20, 16);
  u8g2.print("R:");
  u8g2.print(roll_vis*RAD2DEG,0);
  u8g2.print(" P:");
  u8g2.print(pitch_vis*RAD2DEG,0);
  u8g2.print(" Y:");
  u8g2.print(yaw_vis*RAD2DEG,0);
  // Valores X, Y, Z nas laterais do cubo
  // Lado esquerdo - Acelerômetro
  u8g2.setCursor(0, 40);
  u8g2.print("X:");
  u8g2.print(a.acceleration.x/G, 2);
  u8g2.setCursor(0, 48);
  u8g2.print("Y:");
  u8g2.print(a.acceleration.y/G, 2);
  u8g2.setCursor(0, 56);
  u8g2.print("Z:");
  u8g2.print(a.acceleration.z/G, 2);
  
  // Lado direito - Giroscópio
  u8g2.setCursor(90, 40);
  u8g2.print("X:");
  u8g2.print(gx*RAD2DEG, 1);
  u8g2.setCursor(90, 48);
  u8g2.print("Y:");
  u8g2.print(gy*RAD2DEG, 1);
  u8g2.setCursor(90, 56);
  u8g2.print("Z:");
  u8g2.print(gz*RAD2DEG, 1);
  

  u8g2.sendBuffer();
}

