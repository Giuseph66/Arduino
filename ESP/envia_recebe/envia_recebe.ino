#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <IRremote.hpp>   // Biblioteca IRremote (Armin Joachimsmeyer)

// --- Pinos ---
#define IR_LED_PIN 19   // "S" do módulo IR (emissor)
#define IR_RECEIVE_PIN 18   // OUT do receptor → D18
#define I2C_SDA    21
#define I2C_SCL    22

// OLED 128x64 I2C (SSD1306)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA);

// Estados do sistema
enum State { SENDING, RECEIVING };
State currentState = SENDING;

// Intervalo e repetições para envio
const uint16_t GAP_MS = 3000;  // Aumentado para dar tempo de receber
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

// Variáveis para controle de tempo
unsigned long lastSendTime = 0;
unsigned long lastReceiveTime = 0;
const unsigned long RECEIVE_TIMEOUT = 5000; // 5 segundos para receber

// Utils
String hex32(uint32_t v) {
  char buf[11]; snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)v);
  String s(buf); int i = s.indexOf('x') + 1;
  while (i < (int)s.length() - 1 && s[i] == '0') i++;
  return "0x" + s.substring(i);
}

void showSend(const char* title, const char* proto, uint16_t bits, const String& code) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12); u8g2.print(title);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 28); u8g2.print("Proto: "); u8g2.print(proto);
  u8g2.setCursor(0, 40); u8g2.print("Bits : "); u8g2.print(bits);
  u8g2.setCursor(0, 52); u8g2.print("Code : "); u8g2.print(code);
  u8g2.sendBuffer();
}

void showReceive(const char* proto, uint16_t bits, const String& code, const String& address) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12); u8g2.print("RECEIVED IR");

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 28); u8g2.print("Proto: "); u8g2.print(proto);
  u8g2.setCursor(0, 40); u8g2.print("Addr : "); u8g2.print(address);
  u8g2.setCursor(0, 52); u8g2.print("Code : "); u8g2.print(code);
  u8g2.sendBuffer();
}

void showWaiting() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12); u8g2.print("Aguardando IR...");
  u8g2.setCursor(0, 28); u8g2.print("Aponte controle");
  u8g2.setCursor(0, 44); u8g2.print("Volta em: ");
  u8g2.print((RECEIVE_TIMEOUT - (millis() - lastReceiveTime)) / 1000);
  u8g2.print("s");
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
      showSend("IR TEST", "NEC", c.bits, code);
      break;

    case Cmd::SAMSUNG:
      for (uint8_t i=0;i<REPEATS;i++) sendSamsungPacked(c.data, 0); // repeats=0 (já repetimos no loop)
      showSend("IR TEST", "Samsung", c.bits, code);
      break;

    case Cmd::SONY12:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendSony(c.data, c.bits);
      showSend("IR TEST", "Sony12", c.bits, code);
      break;

    case Cmd::RC5:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendRC5(c.data, c.bits);
      showSend("IR TEST", "RC5", c.bits, code);
      break;

    case Cmd::RC6:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendRC6(c.data, c.bits);
      showSend("IR TEST", "RC6", c.bits, code);
      break;

    case Cmd::JVC:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendJVC(c.data, c.bits, false);
      showSend("IR TEST", "JVC", c.bits, code);
      break;

    case Cmd::LG:
      for (uint8_t i=0;i<REPEATS;i++) IrSender.sendNEC(c.data, c.bits);
      showSend("IR TEST", "LG(NEC)", c.bits, code);
      break;
  }
  Serial.printf("TX %s %u bits: %s\n", c.label, c.bits, code.c_str());
}

void handleReceivedIR() {
  if (IrReceiver.decode()) {
    // Mostra resumo + como reenviar
    IrReceiver.printIRResultShort(&Serial);
    IrReceiver.printIRSendUsage(&Serial);

    // Obtém informações do sinal recebido
    String protoName = getProtocolString(IrReceiver.decodedIRData.protocol);
    String address = hex32(IrReceiver.decodedIRData.address);
    String command = hex32(IrReceiver.decodedIRData.command);
    
    // Mostra no OLED
    showReceive(protoName.c_str(), IrReceiver.decodedIRData.numberOfBits, command, address);
    
    // Reenvia automaticamente se for NEC ou SAMSUNG
    switch (IrReceiver.decodedIRData.protocol) {
      case NEC:
        IrSender.sendNEC(IrReceiver.decodedIRData.address,
                         IrReceiver.decodedIRData.command, 0);
        Serial.println("Reenviado NEC");
        break;
      case SAMSUNG:
        IrSender.sendSamsung(IrReceiver.decodedIRData.address,
                             IrReceiver.decodedIRData.command, 0);
        Serial.println("Reenviado SAMSUNG");
        break;
      default:
        Serial.println("Protocolo não suportado para reenvio");
        break;
    }

    IrReceiver.resume(); // pronto para o próximo código
    lastReceiveTime = millis(); // atualiza tempo do último recebimento
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "IR Emissor/Receptor");
  u8g2.drawStr(0, 28, "ESP32 - D18/D19");
  u8g2.drawStr(0, 44, "Lib: IRremote (Armin)");
  u8g2.sendBuffer();

  // Inicia emissor e receptor
  IrSender.begin(IR_LED_PIN); // inicia emissor (38 kHz padrao)
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // pisca o LED da placa
  
  Serial.println("Sistema IR Emissor/Receptor iniciado");
  Serial.println("D18: Receptor IR");
  Serial.println("D19: Emissor IR");
  Serial.println("Aponte um controle remoto para D18...");
  
  delay(2000);
}

void loop() {
  unsigned long currentTime = millis();
  
  // Verifica se recebeu algum sinal IR
  if (IrReceiver.decode()) {
    currentState = RECEIVING;
    lastReceiveTime = currentTime;
    handleReceivedIR();
  }
  
  // Lógica de mudança de estado
  if (currentState == RECEIVING) {
    // Se passou muito tempo sem receber, volta para envio
    if (currentTime - lastReceiveTime > RECEIVE_TIMEOUT) {
      currentState = SENDING;
      Serial.println("Voltando para modo envio...");
    } else {
      // Mostra contador regressivo
      showWaiting();
    }
  }
  
  // Modo envio - envia comandos de teste
  if (currentState == SENDING) {
    if (currentTime - lastSendTime > GAP_MS) {
      static uint8_t i = 0;
      sendCmd(tests[i]);
      i = (i + 1) % N;
      lastSendTime = currentTime;
    }
  }
}
