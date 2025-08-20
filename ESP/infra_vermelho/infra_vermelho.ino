#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <IRremote.hpp>   // Biblioteca IRremote (Armin Joachimsmeyer)

// --- Pinos ---
#define IR_LED_PIN 19   // "S" do módulo IR
#define I2C_SDA    21
#define I2C_SCL    22

// OLED 128x64 I2C (SSD1306)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA);

// Intervalo e repetições
const uint16_t GAP_MS = 1500;
const uint8_t  REPEATS = 2;

// Comandos de teste
struct Cmd {
  enum Proto { NEC, SAMSUNG, SONY12, RC5, RC6, JVC, LG } p;
  const char* label;
  uint32_t data;   // para Samsung guardamos "packed": 0xAAAA CCCC (A=address, C=command)
  uint16_t bits;
};

Cmd tests[] = {
  { Cmd::NEC,     "NEC/LG  POWER", 0x20DF10EF, 32 },
  { Cmd::SAMSUNG, "Samsung POWER",  0xE0E040BF, 32 }, // addr=0xE0E0, cmd=0x40BF
  { Cmd::SONY12,  "Sony12  POWER",  0x0A90,     12 },
  { Cmd::RC5,     "RC5     POWER",  0x00C,      12 },
  { Cmd::RC6,     "RC6     POWER",  0x00C,      20 },
  { Cmd::JVC,     "JVC     POWER",  0xC5D8,     16 },
  { Cmd::LG,      "LG NEC  POWER",  0x20DF10EF, 32 }, // LG também usa NEC
};
const uint8_t N = sizeof(tests) / sizeof(tests[0]);

// Utils
String hex32(uint32_t v) {
  char buf[11]; snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)v);
  String s(buf); int i = s.indexOf('x') + 1;
  while (i < (int)s.length() - 1 && s[i] == '0') i++;
  return "0x" + s.substring(i);
}

void show(const char* title, const char* proto, uint16_t bits, const String& code) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12); u8g2.print(title);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 28); u8g2.print("Proto: "); u8g2.print(proto);
  u8g2.setCursor(0, 40); u8g2.print("Bits : "); u8g2.print(bits);
  u8g2.setCursor(0, 52); u8g2.print("Code : "); u8g2.print(code);
  u8g2.sendBuffer();
}

// envia Samsung da lib "IRremote (Armin)": separa address/command do packed 32-bit
static inline void sendSamsungPacked(uint32_t packed, int_fast8_t repeats) {
  uint16_t address = (packed >> 16) & 0xFFFF;
  uint16_t command =  packed        & 0xFFFF;
  IrSender.sendSamsung(address, command, repeats);
}

void sendCmd(const Cmd& c) {
  String code = hex32(c.data);
  switch (c.p) {
    case Cmd::NEC:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendNEC(c.data, c.bits);
      show("IR TEST", "NEC", c.bits, code);
      break;

    case Cmd::SAMSUNG:
      for (uint8_t i=0;i<REPEATS;i++) sendSamsungPacked(c.data, 0); // repeats=0 (já repetimos no loop)
      show("IR TEST", "Samsung", c.bits, code);
      break;

    case Cmd::SONY12:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendSony(c.data, c.bits);
      show("IR TEST", "Sony12", c.bits, code);
      break;

    case Cmd::RC5:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendRC5(c.data, c.bits);
      show("IR TEST", "RC5", c.bits, code);
      break;

    case Cmd::RC6:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendRC6(c.data, c.bits);
      show("IR TEST", "RC6", c.bits, code);
      break;

    case Cmd::JVC:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendJVC(c.data, c.bits, false);
      show("IR TEST", "JVC", c.bits, code);
      break;

    case Cmd::LG:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendNEC(c.data, c.bits);
      show("IR TEST", "LG(NEC)", c.bits, code);
      break;
  }
  Serial.printf("TX %s %u bits: %s\n", c.label, c.bits, code.c_str());
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "Teste IR - ESP32");
  u8g2.drawStr(0, 28, "Lib: IRremote (Armin)");
  u8g2.sendBuffer();

  IrSender.begin(IR_LED_PIN); // inicia emissor (38 kHz padrao)
  delay(400);
}

void loop() {
  static uint8_t i = 0;
  sendCmd(tests[i]);
  i = (i + 1) % N;
  delay(GAP_MS);
}
