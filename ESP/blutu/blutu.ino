#include <Wire.h>
#include <U8g2lib.h>
#include "BluetoothSerial.h"
#include <Keypad.h>

// ====== OLED ======
#define I2C_SDA 21
#define I2C_SCL 22
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA
);

// ====== KEYPAD 4x4 ======
const byte LINHAS = 4;
const byte COLUNAS = 4;

char teclas[LINHAS][COLUNAS] = {
  {'1','2','3','4'},
  {'5','6','7','8'},
  {'9','A','B','C'},
  {'D','E','F','G'}
};

byte pinosLinhas[LINHAS]   = {27, 14, 12, 13};
byte pinosColunas[COLUNAS] = {25, 26, 33, 32};

Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// ====== BLUETOOTH SERIAL ======
BluetoothSerial SerialBT;

// ====== Configurações ======
const char* BT_NAME = "Relogio";
const char* BT_PIN  = "0000"; // PIN fixo para exibir na tela

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// Ponteamento opcional:
const bool FORWARD_USB_TO_BT = true;  // o que digitar no Monitor Serial vai para o BT
const bool FORWARD_BT_TO_USB = true;  // o que chegar do BT aparece no Monitor Serial

// Buffer de linha (entrada vinda do BT)
String rxLine;

// ====== ESTADOS DO SISTEMA ======
enum SystemState { 
  STATE_IDLE,           // Aguardando conexão
  STATE_CONNECTED,      // Conectado ao celular
  STATE_COMMAND_MODE,   // Modo de comando
  STATE_RESPONSE_MODE   // Modo de resposta
};

SystemState currentState = STATE_IDLE;

// ====== CONTADORES ======
unsigned long connectionTime = 0;
unsigned long lastActivity = 0;
int commandCount = 0;
int responseCount = 0;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// ====== Utilidades JSON simples ======
void sendJsonKV(const char* k, const char* v) {
  SerialBT.print("{\"");
  SerialBT.print(k);
  SerialBT.print("\":\"");
  SerialBT.print(v);
  SerialBT.println("\"}");
}

void sendJsonKV(const char* k, int v) {
  SerialBT.print("{\"");
  SerialBT.print(k);
  SerialBT.print("\":");
  SerialBT.print(v);
  SerialBT.println("}");
}

void sendJsonStatus() {
  SerialBT.print("{\"ok\":true,\"uptime_ms\":");
  SerialBT.print(millis());
  SerialBT.print(",\"led\":");
  SerialBT.print(digitalRead(LED_BUILTIN) ? 1 : 0);
  SerialBT.print(",\"free_memory\":");
  SerialBT.print(ESP.getFreeHeap());
  SerialBT.println("}");
}

// ====== Processamento de comandos por linha ======
void handleCommand(const String& line) {
  // Formato: CMD [args...]
  // Ex.: "LED 1", "PING", "STATUS", "ECHO oi"
  // Quebra em 1ª palavra + resto
  String cmd, args;
  int sp = line.indexOf(' ');
  if (sp >= 0) {
    cmd  = line.substring(0, sp);
    args = line.substring(sp + 1);
  } else {
    cmd  = line;
    args = "";
  }

  cmd.trim(); cmd.toUpperCase();
  commandCount++;
  lastActivity = millis();
  currentState = STATE_COMMAND_MODE;

  if (cmd == "PING") {
    // {"pong":1}
    SerialBT.println("{\"pong\":1}");
    responseCount++;
  }
  else if (cmd == "LED") {
    args.trim();
    int v = args.toInt();
    digitalWrite(LED_BUILTIN, v ? HIGH : LOW);
    // {"ok":true,"led":0/1}
    SerialBT.print("{\"ok\":true,\"led\":");
    SerialBT.print(v ? 1 : 0);
    SerialBT.println("}");
    responseCount++;
  }
  else if (cmd == "STATUS") {
    sendJsonStatus();
    responseCount++;
  }
  else if (cmd == "MEMORY") {
    SerialBT.print("{\"memory\":");
    SerialBT.print(ESP.getFreeHeap());
    SerialBT.println("}");
    responseCount++;
  }
  else if (cmd == "ECHO") {
    // {"echo":"<args>"}
    SerialBT.print("{\"echo\":\"");
    // escapinho simples de aspas duplas (não robusto, mas quebra o galho)
    for (char c : args) {
      if (c == '\"') SerialBT.print("\\\"");
      else if (c == '\\') SerialBT.print("\\\\");
      else SerialBT.print(c);
    }
    SerialBT.println("\"}");
    responseCount++;
    
    // Exibe o texto do echo no display por 3 segundos
    u8g2.clearBuffer();
    drawHeader("ECHO RECEBIDO");
    
    u8g2.setFont(u8g2_font_7x13_tf);
    u8g2.setCursor(0, 30);
    u8g2.print("Texto:");
    u8g2.setCursor(0, 45);
    u8g2.print(args);
    
    u8g2.sendBuffer();
    delay(3000); // Mostra por 3 segundos
  }
  else {
    // {"error":"unknown_cmd","cmd":"..."}
    SerialBT.print("{\"error\":\"unknown_cmd\",\"cmd\":\"");
    SerialBT.print(cmd);
    SerialBT.println("\"}");
    responseCount++;
  }

  currentState = STATE_RESPONSE_MODE;
  delay(100); // Pequena pausa para mostrar resposta
  currentState = STATE_CONNECTED;
}

// ====== INTERFACE OLED ======
void drawHeader(const char* title) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 8);
  u8g2.print(title);
  u8g2.drawHLine(0, 10, 128);
}

void drawStatusBar() {
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 62);
  u8g2.print("5:Restart  A/B/C/D:Comandos");
}

void drawIdleScreen() {
  u8g2.clearBuffer();
  drawHeader("BT SERIAL SERVER");
  
  u8g2.setFont(u8g2_font_7x13_tf);
  u8g2.setCursor(0, 30);
  u8g2.print("Aguardando");
  u8g2.setCursor(0, 45);
  u8g2.print("conexao...");
  
  drawStatusBar();
  u8g2.sendBuffer();
}

void drawConnectedScreen() {
  u8g2.clearBuffer();
  drawHeader("CONECTADO");
  
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Status da conexão
  u8g2.setCursor(0, 22);
  u8g2.print("Status: Conectado");
  
  // Tempo de conexão
  u8g2.setCursor(0, 32);
  u8g2.print("Tempo: ");
  u8g2.print((millis() - connectionTime) / 1000);
  u8g2.print("s");
  
  // Contadores
  u8g2.setCursor(0, 42);
  u8g2.print("Comandos: ");
  u8g2.print(commandCount);
  
  u8g2.setCursor(0, 52);
  u8g2.print("Respostas: ");
  u8g2.print(responseCount);
  
  drawStatusBar();
  u8g2.sendBuffer();
}

void drawCommandScreen() {
  u8g2.clearBuffer();
  drawHeader("COMANDO RECEBIDO");
  
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Comando recebido
  u8g2.setCursor(0, 22);
  u8g2.print("Cmd: ");
  String cmd = rxLine;
  if (cmd.length() > 15) cmd = cmd.substring(0, 15);
  u8g2.print(cmd);
  
  // Processando resposta
  u8g2.setCursor(0, 32);
  u8g2.print("Processando...");
  
  // Última atividade
  u8g2.setCursor(0, 42);
  u8g2.print("Atividade: ");
  u8g2.print((millis() - lastActivity) / 1000);
  u8g2.print("s");
  
  drawStatusBar();
  u8g2.sendBuffer();
}

void drawResponseScreen() {
  u8g2.clearBuffer();
  drawHeader("RESPOSTA ENVIADA");
  
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Resposta enviada
  u8g2.setCursor(0, 22);
  u8g2.print("Resp: JSON enviado");
  
  // Status
  u8g2.setCursor(0, 32);
  u8g2.print("Status: Enviado");
  
  // Contadores
  u8g2.setCursor(0, 42);
  u8g2.print("Total: ");
  u8g2.print(commandCount);
  u8g2.print("/");
  u8g2.print(responseCount);
  
  drawStatusBar();
  u8g2.sendBuffer();
}

// ====== SETUP ======
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  delay(200);

  // OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "Iniciando BT Serial");
  u8g2.sendBuffer();
  
  // Keypad
  teclado.setDebounceTime(25);

  // Inicia Bluetooth Serial
  // SerialBT.setPin(BT_PIN, strlen(BT_PIN)); // PIN opcional
  // SerialBT.setPin(NULL, 0); // Sem PIN
  
  // Configurações para evitar PIN automático
  // esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  
  if (!SerialBT.begin(BT_NAME)) {
    Serial.println("Falha ao iniciar Bluetooth Serial! Reinicie o ESP32.");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 12, "Erro: BT nao iniciou");
    u8g2.drawStr(0, 28, "Reinicie o ESP32");
    u8g2.sendBuffer();
    while (1) delay(1000);
  }

  Serial.println();
  Serial.println("=== ESP32 Bluetooth Serial ===");
  Serial.print("Dispositivo: "); Serial.println(BT_NAME);
  Serial.print("PIN: "); Serial.println(BT_PIN);
  Serial.println("Comandos: PING | LED 1/0 | STATUS | MEMORY | ECHO <texto>");
  Serial.println("Conecte-se via app BT serial e envie linhas terminadas em \\n.");
  
  currentState = STATE_IDLE;
  drawIdleScreen();
}

// ====== LOOP ======
void loop() {
  // Verifica conexão Bluetooth
  deviceConnected = SerialBT.hasClient();
  
  if (deviceConnected && !oldDeviceConnected) {
    connectionTime = millis();
    currentState = STATE_CONNECTED;
    Serial.println("Dispositivo conectado!");
  }
  
  if (!deviceConnected && oldDeviceConnected) {
    currentState = STATE_IDLE;
    Serial.println("Dispositivo desconectado!");
  }
  
  oldDeviceConnected = deviceConnected;

  // Processa teclas do keypad
  char k = teclado.getKey();
  if (k != NO_KEY) {
    Serial.print("Tecla pressionada: ");
    Serial.println(k);
    
    // Comandos locais via keypad
    switch (k) {
      case '5':
        // Força restart do Bluetooth
        Serial.println("Reiniciando Bluetooth...");
        SerialBT.end();
        delay(1000);
        SerialBT.begin(BT_NAME, true);
        Serial.println("Bluetooth reiniciado!");
        break;
      case 'A':
        if (deviceConnected) {
          SerialBT.println("{\"local_cmd\":\"A\"}");
        }
        break;
      case 'B':
        if (deviceConnected) {
          SerialBT.println("{\"local_cmd\":\"B\"}");
        }
        break;
      case 'C':
        if (deviceConnected) {
          SerialBT.print("{\"status\":\"");
          SerialBT.print(ESP.getFreeHeap());
          SerialBT.println(" bytes free\"}");
        }
        break;
      case 'D':
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        if (deviceConnected) {
          SerialBT.print("{\"led\":");
          SerialBT.print(digitalRead(LED_BUILTIN) ? 1 : 0);
          SerialBT.println("}");
        }
        break;
    }
  }

  // ====== Recepção via Bluetooth (por bytes) -> montar linha e processar ======
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();
    if (FORWARD_BT_TO_USB) Serial.write(c); // espelha no USB (debug)

    if (c == '\r') continue;
    if (c == '\n') {
      rxLine.trim();
      if (!rxLine.isEmpty()) {
        handleCommand(rxLine);
        
        // Mostra tela de resposta por 2 segundos
        drawResponseScreen();
        delay(2000);
        currentState = STATE_CONNECTED;
      }
      rxLine = "";
    } else {
      if (rxLine.length() < 512) rxLine += c; // limite simples
    }
  }

  // ====== (Opcional) Tudo que você digitar no USB vai para o Bluetooth ======
  if (FORWARD_USB_TO_BT && deviceConnected) {
    while (Serial.available()) {
      int c = Serial.read();
      SerialBT.write(c);
    }
  }

  // Atualiza interface baseado no estado
  switch (currentState) {
    case STATE_IDLE:
      if (millis() % 5000 < 100) { // Atualiza a cada 5 segundos
        drawIdleScreen();
      }
      break;
      
    case STATE_CONNECTED:
      if (millis() % 1000 < 100) { // Atualiza a cada segundo
        drawConnectedScreen();
      }
      break;
      
    case STATE_COMMAND_MODE:
      drawCommandScreen();
      break;
      
    case STATE_RESPONSE_MODE:
      // Já tratado acima
      break;
  }

  delay(10);
}
