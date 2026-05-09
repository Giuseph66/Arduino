#include <Arduino.h>
#include <Adafruit_Fingerprint.h>

// UART2 no ESP32: RX2=GPIO16, TX2=GPIO17 (padrão na maioria dos DevKit)
HardwareSerial FPSerial(2);  // UART2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FPSerial);

// Opcional: pino de toque (TCH) pra saber se tem dedo no sensor
const int PIN_TCH = 4; // ligue TCH aqui se quiser; senão, ignore

// Escolha a velocidade do sensor (a maioria usa 57600)
const uint32_t FP_BAUD = 57600;

// Variáveis para controle de debounce e detecção de borda do TCH
bool lastTch = false;                 // estado anterior do TCH
const uint32_t DEBOUNCE_MS = 80;      // debounce simples
uint32_t lastEdgeMs = 0;

void waitFingerPlace();
void doEnroll(uint16_t id);
void doSearch();

void setup() {
  Serial.begin(115200);
  delay(200);

  // Se o TCH do seu módulo "sobe" quando tem dedo, use PULLDOWN;
  // Se ele "desce" (ativo em LOW), mude para INPUT_PULLUP e ajuste ACTIVE_LEVEL.
  pinMode(PIN_TCH, INPUT_PULLDOWN);

  // Inicia UART2 com pinos fixos RX=16, TX=17
  FPSerial.begin(FP_BAUD, SERIAL_8N1, 16, 17);
  delay(50);

  finger.begin(FP_BAUD);
  delay(100);

  Serial.println(F("\n[ESP32 + Fingerprint @3.3V via VA]"));
  if (finger.verifyPassword()) {
    Serial.println(F("Sensor OK (password verificada)."));
  } else {
    Serial.println(F("Falha ao comunicar com o sensor. Verifique pinos e 3V3 no VA."));
    // não trava; deixa continuar para debug
  }

  finger.getParameters();
  Serial.print(F("Status register: ")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("System ID: "));      Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));       Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);

  finger.getTemplateCount();
  Serial.print(F("Templates salvos: ")); Serial.println(finger.templateCount);

  Serial.println(F("\nComandos pela Serial:"));
  Serial.println(F("  e <ID>  -> cadastrar digital no ID (1..200)"));
  Serial.println(F("  v       -> verificar/buscar digital ao vivo"));
  Serial.println(F("Ex.: digite: e 5   (e Enter)"));
}

void loop() {
  // Console de comandos simples
  if (Serial.available()) {
    char c = Serial.read();

    if (c == 'e') {
      // ler ID após espaço
      while (Serial.available() == 0) {}
      int id = Serial.parseInt();
      if (id <= 0) {
        Serial.println(F("ID inválido. Use 1..200 (depende da capacidade)."));
      } else {
        doEnroll((uint16_t)id);
      }
    } else if (c == 'v') {
      doSearch();
    }
  }

  // ====== Leitura correta do TCH (edge detect + debounce) ======
  bool tchNow = digitalRead(PIN_TCH);  // lê o nível do pino
  
  // Debug: mostra o estado do TCH a cada 2 segundos
  static uint32_t lastDebugMs = 0;
  uint32_t now = millis();
  if (now - lastDebugMs > 2000) {
    Serial.print(F("[DEBUG] TCH = ")); Serial.println(tchNow ? "HIGH" : "LOW");
    lastDebugMs = now;
  }

  // Dispara apenas quando sobe de LOW->HIGH, com debounce
  if (!lastTch && tchNow) {
    if (now - lastEdgeMs > DEBOUNCE_MS) {
      Serial.println(F("[TCH] Toque detectado. Iniciando verificação..."));
      doSearch();
      lastEdgeMs = now;
    }
  }
  lastTch = tchNow;

  // ====== ALTERNATIVA: Detecção automática sem TCH ======
  // Verifica se há dedo no sensor a cada 1 segundo
  static uint32_t lastAutoCheckMs = 0;
  if (now - lastAutoCheckMs > 1000) {
    // Tenta capturar imagem rapidamente
    uint8_t result = finger.getImage();
    if (result == FINGERPRINT_OK) {
      Serial.println(F("[AUTO] Dedo detectado no sensor. Iniciando verificação..."));
      doSearch();
    }
    lastAutoCheckMs = now;
  }

  // Pequena folga para não saturar o loop
  delay(5);
}

/** Fluxo de cadastro (enroll): captura duas vezes e grava no ID */
void doEnroll(uint16_t id) {
  Serial.print(F("\n[ENROLL] Iniciando cadastro no ID ")); Serial.println(id);
  finger.getTemplateCount();
  Serial.print(F("Templates atuais: ")); Serial.println(finger.templateCount);

  // 1ª captura
  Serial.println(F("Coloque o dedo no sensor..."));
  while (finger.getImage() != FINGERPRINT_OK) { delay(20); }
  if (finger.image2Tz(1) != FINGERPRINT_OK) { Serial.println(F("Falha em image2Tz #1.")); return; }
  Serial.println(F("Remova o dedo."));
  delay(800);
  while (finger.getImage() != FINGERPRINT_NOFINGER) { delay(20); }

  // 2ª captura
  Serial.println(F("Coloque o MESMO dedo novamente..."));
  while (finger.getImage() != FINGERPRINT_OK) { delay(20); }
  if (finger.image2Tz(2) != FINGERPRINT_OK) { Serial.println(F("Falha em image2Tz #2.")); return; }

  // Cria modelo e salva
  if (finger.createModel() != FINGERPRINT_OK) { Serial.println(F("As duas leituras não combinaram.")); return; }
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    Serial.print(F("Cadastro concluído no ID ")); Serial.println(id);
  } else {
    Serial.println(F("Falha ao salvar template."));
  }
}

/** Busca/Verifica: captura dedo e procura no banco do módulo */
void doSearch() {
  Serial.println(F("\n[VERIFY] Encoste o dedo no sensor..."));
  
  // Aguarda dedo (só se não foi detectado automaticamente)
  uint8_t result = finger.getImage();
  if (result != FINGERPRINT_OK) {
    // Se não há dedo, aguarda até ter
    while (finger.getImage() != FINGERPRINT_OK) {
      delay(20);
    }
  }
  
  if (finger.image2Tz() != FINGERPRINT_OK) {
    Serial.println(F("Falha ao processar imagem."));
    return;
  }
  if (finger.fingerSearch() == FINGERPRINT_OK) {
    Serial.print(F("Encontrado! ID: "));
    Serial.print(finger.fingerID);
    Serial.print(F("  Confiança: "));
    Serial.println(finger.confidence);
  } else {
    Serial.println(F("Não encontrado."));
  }
}
