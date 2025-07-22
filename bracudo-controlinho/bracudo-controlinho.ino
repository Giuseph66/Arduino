#include <Wire.h>
#include <U8g2lib.h>
#include <Servo.h>

// Construtor para I2C com Nano (A4 = SDA, A5 = SCL/SCK)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Mapeamento dos pinos dos 3 joysticks (apenas X e Y)
const uint8_t J1_X_PIN = A0;
const uint8_t J1_Y_PIN = A1;
const uint8_t J2_X_PIN = A2;
const uint8_t J2_Y_PIN = A3;
const uint8_t J3_X_PIN = A6; // A6 e A7 são analógicos somente
const uint8_t J3_Y_PIN = A7;

// Pino do servo da garra
const uint8_t SERVO_PIN = 4;
Servo servoGarra;

// === Configuração dos motores de passo (6 eixos) ===
const uint8_t NUM_MOTORES = 6;
const uint8_t enablePins[NUM_MOTORES] = {5, 8, 11, 49, 43, 37};
const uint8_t dirPins[NUM_MOTORES]    = {6, 9, 12, 51, 45, 39};
const uint8_t stepPins[NUM_MOTORES]   = {7, 10, 13, 53, 47, 41};

enum Direcao : uint8_t { CW = HIGH, CCW = LOW };

const uint16_t VEL_US_MIN = 200;   // joystick no limite -> mais rápido
const uint16_t VEL_US_MAX = 800;   // joystick perto do centro -> mais devagar
const int DEAD_BAND = 80;          // zona morta do joystick

const long LIMITE_MIN[NUM_MOTORES] = {-1000, -1000, -1000, -1000, -1000, -1000};
const long LIMITE_MAX[NUM_MOTORES] = { 1000,  1000,  1000,  1000,  1000,  1000};

long posMotor[NUM_MOTORES] = {0};
unsigned long ultimoPulso[NUM_MOTORES] = {0};

const uint8_t PAREADO_A = 1;  // motor id1
const uint8_t PAREADO_B = 2;  // motor id2

inline void habilitaMotor(uint8_t idx, bool habilitar) {
  digitalWrite(enablePins[idx], habilitar ? LOW : HIGH); // LOW = enable nos drivers
}

inline void defineDirecao(uint8_t idx, Direcao dir) {
  digitalWrite(dirPins[idx], dir);
}

inline void pulsoMotor(uint8_t idx) {
  digitalWrite(stepPins[idx], HIGH);
  delayMicroseconds(2);
  digitalWrite(stepPins[idx], LOW);
}

void setup() {
  u8g2.begin();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 8, "Bracudo Controlinho");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  Serial.begin(115200);
  servoGarra.attach(SERVO_PIN);
  servoGarra.write(0); // fecha garra inicialmente
  for (uint8_t i = 0; i < NUM_MOTORES; i++) {
    pinMode(enablePins[i], OUTPUT);
    pinMode(dirPins[i], OUTPUT);
    pinMode(stepPins[i], OUTPUT);
    habilitaMotor(i, true);
    defineDirecao(i, CW);
  }
}

void loop() {
  // Leitura dos eixos dos joysticks
  int j1x = analogRead(J1_X_PIN);
  int j1y = analogRead(J1_Y_PIN);
  int j2x = analogRead(J2_X_PIN);
  int j2y = analogRead(J2_Y_PIN);
  int j3x = analogRead(J3_X_PIN);
  int j3y = analogRead(J3_Y_PIN);
  
  // === Atualização periódica do display e debug ===
  static unsigned long tDisplay = 0;
  if (millis() - tDisplay >= 100) { // a cada 100 ms
    tDisplay = millis();

    Serial.print("J1X:"); Serial.print(j1x);
    Serial.print(" J1Y:"); Serial.print(j1y);
    Serial.print(" J2X:"); Serial.print(j2x);
    Serial.print(" J2Y:"); Serial.print(j2y);
    Serial.print(" J3X:"); Serial.print(j3x);
    Serial.print(" J3Y:"); Serial.println(j3y);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    char buf[16];
    sprintf(buf, "J1X: %4d", j1x); u8g2.drawStr(0, 8, buf);
    sprintf(buf, "J1Y: %4d", j1y); u8g2.drawStr(0, 16, buf);
    sprintf(buf, "J2X: %4d", j2x); u8g2.drawStr(0, 24, buf);
    sprintf(buf, "J2Y: %4d", j2y); u8g2.drawStr(0, 32, buf);
    sprintf(buf, "J3X: %4d", j3x); u8g2.drawStr(0, 40, buf);
    sprintf(buf, "J3Y: %4d", j3y); u8g2.drawStr(0, 48, buf);
    u8g2.sendBuffer();
  }

  // === Controle da garra (servo) ===
  uint8_t angGarra = map(j3y, 0, 1023, 0, 180);
  servoGarra.write(angGarra);

  // === Controle dos motores a partir dos joysticks ===
  unsigned long agora = micros();

  // ---- Par de motores id1 & id2 controlados por J1X (direções opostas) ----
  {
    int delta = j1x - 512;
    if (abs(delta) >= DEAD_BAND) {
      Direcao dirA = (delta > 0) ? CW : CCW;
      Direcao dirB = (dirA == CW) ? CCW : CW;
      long novaPosA = posMotor[PAREADO_A] + ((dirA == CW) ? 1 : -1);
      long novaPosB = posMotor[PAREADO_B] + ((dirB == CW) ? 1 : -1);
      if (novaPosA >= LIMITE_MIN[PAREADO_A] && novaPosA <= LIMITE_MAX[PAREADO_A] &&
          novaPosB >= LIMITE_MIN[PAREADO_B] && novaPosB <= LIMITE_MAX[PAREADO_B]) {
        uint16_t vel_us = map(abs(delta), DEAD_BAND, 512, VEL_US_MAX, VEL_US_MIN);
        if (agora - ultimoPulso[PAREADO_A] >= (unsigned long)vel_us * 2) {
          defineDirecao(PAREADO_A, dirA);
          defineDirecao(PAREADO_B, dirB);
          pulsoMotor(PAREADO_A);
          pulsoMotor(PAREADO_B);
          posMotor[PAREADO_A] = novaPosA;
          posMotor[PAREADO_B] = novaPosB;
          ultimoPulso[PAREADO_A] = ultimoPulso[PAREADO_B] = agora;
        }
      }
    }
  }

  // ---- Motor id0 controlado por J1Y ----
  {
    int delta = j1y - 512;
    uint8_t m = 0;
    if (abs(delta) >= DEAD_BAND) {
      Direcao dir = (delta > 0) ? CW : CCW;
      long novaPos = posMotor[m] + ((dir == CW) ? 1 : -1);
      if (novaPos >= LIMITE_MIN[m] && novaPos <= LIMITE_MAX[m]) {
        uint16_t vel_us = map(abs(delta), DEAD_BAND, 512, VEL_US_MAX, VEL_US_MIN);
        if (agora - ultimoPulso[m] >= (unsigned long)vel_us * 2) {
          defineDirecao(m, dir);
          pulsoMotor(m);
          posMotor[m] = novaPos;
          ultimoPulso[m] = agora;
        }
      }
    }
  }

  // ---- Motor id3 controlado por J2X ----
  {
    int delta = j2x - 512;
    uint8_t m = 3;
    if (abs(delta) >= DEAD_BAND) {
      Direcao dir = (delta > 0) ? CW : CCW;
      long novaPos = posMotor[m] + ((dir == CW) ? 1 : -1);
      if (novaPos >= LIMITE_MIN[m] && novaPos <= LIMITE_MAX[m]) {
        uint16_t vel_us = map(abs(delta), DEAD_BAND, 512, VEL_US_MAX, VEL_US_MIN);
        if (agora - ultimoPulso[m] >= (unsigned long)vel_us * 2) {
          defineDirecao(m, dir);
          pulsoMotor(m);
          posMotor[m] = novaPos;
          ultimoPulso[m] = agora;
        }
      }
    }
  }

  // ---- Motor id4 controlado por J2Y ----
  {
    int delta = j2y - 512;
    uint8_t m = 4;
    if (abs(delta) >= DEAD_BAND) {
      Direcao dir = (delta > 0) ? CW : CCW;
      long novaPos = posMotor[m] + ((dir == CW) ? 1 : -1);
      if (novaPos >= LIMITE_MIN[m] && novaPos <= LIMITE_MAX[m]) {
        uint16_t vel_us = map(abs(delta), DEAD_BAND, 512, VEL_US_MAX, VEL_US_MIN);
        if (agora - ultimoPulso[m] >= (unsigned long)vel_us * 2) {
          defineDirecao(m, dir);
          pulsoMotor(m);
          posMotor[m] = novaPos;
          ultimoPulso[m] = agora;
        }
      }
    }
  }

  // ---- Motor id5 controlado por J3X ----
  {
    int delta = j3x - 512;
    uint8_t m = 5;
    if (abs(delta) >= DEAD_BAND) {
      Direcao dir = (delta > 0) ? CW : CCW;
      long novaPos = posMotor[m] + ((dir == CW) ? 1 : -1);
      if (novaPos >= LIMITE_MIN[m] && novaPos <= LIMITE_MAX[m]) {
        uint16_t vel_us = map(abs(delta), DEAD_BAND, 512, VEL_US_MAX, VEL_US_MIN);
        if (agora - ultimoPulso[m] >= (unsigned long)vel_us * 2) {
          defineDirecao(m, dir);
          pulsoMotor(m);
          posMotor[m] = novaPos;
          ultimoPulso[m] = agora;
        }
      }
    }
  }
  // === Fim do controle dos motores ===
}
