// ============================================================
//  Melhor.ino — Jammer 2.4 GHz (ESP32 + 2x nRF24L01+ PA/LNA)
//  Autor: Jesus
//
//  ESTRATÉGIA:
//    Os dois rádios varrem o espectro completo 0-79 de forma
//    independente e defasada, cada um com payload máximo (32 bytes)
//    para maximizar o tempo de emissão RF por canal.
//    Os canais de advertising BLE (2, 26, 80) são martelados
//    separadamente a cada ciclo para impedir reconexão.
//    Sem alternância de modos — sweep contínuo é mais eficaz
//    contra AFH do que padrões previsíveis.
//
//  MODO TESTE (MODO_TESTE true):
//    Radio1 TX → Radio2 RX. Verifica hardware antes de jamming.
//
//  PINOS:
//    HSPI — SCK=14, MISO=25, MOSI=13, CSN=32, CE=33
//    VSPI — SCK=18, MISO=19, MOSI=23, CSN=21, CE=22
// ============================================================

#include <SPI.h>
#include "RF24.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "esp_task_wdt.h"  // watchdog manual

// ------------------------------------------------------------
// FLAG: true = teste de hardware | false = jamming
// ------------------------------------------------------------
#define MODO_TESTE  false

// ------------------------------------------------------------
// Configurações de jamming
// ------------------------------------------------------------
#define CANAL_MIN   0
#define CANAL_MAX   79

// Offset inicial entre os dois rádios — garantem que nunca
// estão no mesmo canal ao mesmo tempo
#define OFFSET_R2   13

// Canais de advertising BLE — martelados separadamente
// ch  2 = 2402 MHz (BLE adv ch 37)
// ch 26 = 2426 MHz (BLE adv ch 38)
// ch 80 = 2480 MHz (BLE adv ch 39)
static const uint8_t CANAIS_ADV[] = {2, 26, 80};
#define N_CANAIS_ADV  3

// Payload máximo — 32 bytes de 0xFF
// Burst mais longo = mais tempo emitindo RF por canal
static const uint8_t PAYLOAD[32] = {
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

// Endereço broadcast — ambos os rádios escrevem aqui
static const uint8_t ADDR_BCAST[3] = {0xFF, 0xFF, 0xFF};

// ------------------------------------------------------------
// Modo teste
// ------------------------------------------------------------
#define CANAL_TESTE         108
#define INTERVALO_TESTE_MS  500
#define TIMEOUT_RX_MS       200
static const uint8_t ENDERECO_TESTE[6] = "JMRX1";

// ------------------------------------------------------------
// SPI e rádios
// ------------------------------------------------------------
#define SPI_VELOCIDADE  16000000

SPIClass *hspi = nullptr;
SPIClass *vspi = nullptr;

RF24 radio1(33, 32, SPI_VELOCIDADE);  // HSPI: CE=33, CSN=32
RF24 radio2(22, 21, SPI_VELOCIDADE);  // VSPI: CE=22, CSN=21

// Canais correntes de cada rádio no sweep
uint8_t ch1 = CANAL_MIN;
uint8_t ch2 = (CANAL_MIN + OFFSET_R2) % (CANAL_MAX + 1);

// Modo teste
uint32_t contadorTX    = 0;
uint32_t contadorRX    = 0;
uint32_t contadorPerda = 0;
uint32_t ultimoEnvio   = 0;

// ------------------------------------------------------------
// Inicializa os barramentos SPI
// ------------------------------------------------------------
void inicializarSPI() {
  Serial.println("\nInicializando HSPI (Radio 1)...");
  hspi = new SPIClass(HSPI);
  hspi->begin(14, 25, 13, 32);

  Serial.println("Inicializando VSPI (Radio 2)...");
  vspi = new SPIClass(VSPI);
  vspi->begin(18, 19, 23, 21);

  delay(100);
}

// ------------------------------------------------------------
// Inicializa rádio para JAMMING
//   - payload 32 bytes (máximo — burst mais longo)
//   - addressWidth 3 (mínimo — overhead mínimo)
//   - AutoAck OFF, CRC OFF, Retries 0
//   - PA_MAX + LNA, 2 Mbps
//   - pipe de escrita broadcast aberto
// ------------------------------------------------------------
bool inicializarRadioJammer(RF24 &radio, SPIClass *spiBus,
                            uint8_t canalInicial, const char *nome) {
  if (!radio.begin(spiBus)) {
    Serial.print(nome);
    Serial.println(": FALHOU ao iniciar!");
    return false;
  }
  delay(200);

  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPayloadSize(32);       // payload máximo = burst mais longo
  radio.setAddressWidth(3);       // endereço mínimo = overhead mínimo
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setChannel(canalInicial);
  radio.openWritingPipe(ADDR_BCAST);
  radio.flush_tx();               // limpa FIFO — chip parte do zero

  Serial.print(nome);
  Serial.println(": OK");
  radio.printPrettyDetails();

  return true;
}

// ------------------------------------------------------------
// Transmite no canal especificado e avança para o próximo
// Flush antes de cada transmissão — evita FIFO cheio travando
// ------------------------------------------------------------
inline void jamCanal(RF24 &radio, uint8_t canal) {
  radio.flush_tx();
  radio.setChannel(canal);
  radio.writeFast(PAYLOAD, sizeof(PAYLOAD));
}

// ------------------------------------------------------------
// Um passo do sweep contínuo:
//   1. Transmite nos canais correntes de cada rádio
//   2. Avança cada rádio para o próximo canal (wrap 0-79)
//   3. Martela os canais de advertising BLE com ambos os rádios
// ------------------------------------------------------------
void passoJammer() {
  // Sweep principal — rádios defasados em OFFSET_R2 canais
  jamCanal(radio1, ch1);
  jamCanal(radio2, ch2);

  ch1 = (ch1 >= CANAL_MAX) ? CANAL_MIN : ch1 + 1;
  ch2 = (ch2 >= CANAL_MAX) ? CANAL_MIN : ch2 + 1;

  // Martela canais de advertising BLE com ambos os rádios
  // Isso impede que dispositivos BT/BLE consigam fazer handshake
  // ou reconectar após perder o link durante o sweep
  for (uint8_t i = 0; i < N_CANAIS_ADV; i++) {
    radio1.flush_tx();
    radio1.setChannel(CANAIS_ADV[i]);
    radio1.writeFast(PAYLOAD, sizeof(PAYLOAD));

    radio2.flush_tx();
    radio2.setChannel(CANAIS_ADV[i]);
    radio2.writeFast(PAYLOAD, sizeof(PAYLOAD));
  }
}

// ------------------------------------------------------------
// Inicialização do modo teste
// ------------------------------------------------------------
bool inicializarRadioTeste(RF24 &radio, SPIClass *spiBus, const char *nome) {
  if (!radio.begin(spiBus)) {
    Serial.print(nome);
    Serial.println(": FALHOU!");
    return false;
  }
  delay(100);
  radio.setChannel(CANAL_TESTE);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW, true);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(true);
  radio.setRetries(5, 15);
  radio.setPayloadSize(sizeof(uint32_t));
  Serial.print(nome);
  Serial.println(": OK (modo teste)");
  radio.printPrettyDetails();
  return true;
}

void setupTeste() {
  Serial.println("\n============================================");
  Serial.println("  MODO TESTE — Radio1 TX | Radio2 RX");
  Serial.println("============================================");
  bool ok1 = inicializarRadioTeste(radio1, hspi, "Radio1 TX");
  delay(200);
  bool ok2 = inicializarRadioTeste(radio2, vspi, "Radio2 RX");
  if (!ok1 || !ok2) {
    Serial.println("ERRO: falha na inicializacao.");
    while (true) delay(1000);
  }
  radio1.stopListening();
  radio1.openWritingPipe(ENDERECO_TESTE);
  radio2.openReadingPipe(1, ENDERECO_TESTE);
  radio2.startListening();
  Serial.print("[TESTE] Canal: ");
  Serial.print(CANAL_TESTE);
  Serial.print(" (");
  Serial.print(2400 + CANAL_TESTE);
  Serial.println(" MHz)\n");
}

void loopTeste() {
  uint32_t agora = millis();
  if (agora - ultimoEnvio < INTERVALO_TESTE_MS) return;
  ultimoEnvio = agora;
  contadorTX++;
  bool ack = radio1.write(&contadorTX, sizeof(contadorTX));
  Serial.print("[TX] #");
  Serial.print(contadorTX);
  Serial.print(ack ? " OK" : " FALHOU");
  uint32_t inicio = millis();
  bool recebido = false;
  uint32_t pacoteRX = 0;
  while (millis() - inicio < TIMEOUT_RX_MS) {
    if (radio2.available()) {
      radio2.read(&pacoteRX, sizeof(pacoteRX));
      recebido = true;
      break;
    }
    delayMicroseconds(100);
  }
  if (recebido) { contadorRX++; Serial.print("  [RX] #"); Serial.print(pacoteRX); }
  else          { Serial.print("  [RX] TIMEOUT"); }
  if (!ack) contadorPerda++;
  float taxa = contadorTX > 0 ? (100.0f * contadorRX / contadorTX) : 0.0f;
  Serial.print("  TX:"); Serial.print(contadorTX);
  Serial.print(" RX:");  Serial.print(contadorRX);
  Serial.print(" Perda:"); Serial.print(contadorPerda);
  Serial.print(" Taxa:"); Serial.print(taxa, 1); Serial.println("%");
}

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  Melhor.ino — Jammer 2.4 GHz");
  Serial.println("  ESP32 + 2x nRF24L01+ PA/LNA");
  Serial.println("============================================");

  Serial.println("Desligando BT e Wi-Fi internos do ESP32...");
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();
  Serial.println("OK.");

  // Desativa o task watchdog — o loop de jamming é intencionalmente
  // intensivo e não pode ceder CPU com frequência suficiente para
  // alimentar o WDT. Sem isso o ESP32 reseta a cada ~3-30 segundos.
  esp_task_wdt_deinit();

  inicializarSPI();

#if MODO_TESTE
  setupTeste();
#else
  Serial.println("\n============================================");
  Serial.println("  MODO JAMMER");
  Serial.println("  Sweep continuo + advertising BLE martelado");
  Serial.println("============================================");

  bool ok1 = inicializarRadioJammer(radio1, hspi, ch1, "Radio1 (HSPI)");
  delay(200);
  bool ok2 = inicializarRadioJammer(radio2, vspi, ch2, "Radio2 (VSPI)");

  if (!ok1 || !ok2) {
    Serial.println("\n!!! ERRO: Um ou mais radios falharam !!!");
    Serial.println("  1. Capacitor 100uF em cada modulo?");
    Serial.println("  2. GND comum entre ESP32 e modulos?");
    Serial.println("  3. Pinos SPI corretos?");
    while (true) delay(1000);
  }

  Serial.println("\n============================================");
  Serial.println("  Ambos OK! Jamming ativo.");
  Serial.println("============================================\n");
#endif
}

// ------------------------------------------------------------
// Loop
// ------------------------------------------------------------
void loop() {
#if MODO_TESTE
  loopTeste();
  return;
#endif

  // Feed do watchdog a cada 80 ciclos (~1 volta completa no espectro)
  // Sem isso o FreeRTOS mata o task e reseta o ESP32
  static uint16_t ciclo = 0;
  if (++ciclo >= 80) {
    ciclo = 0;
    esp_task_wdt_reset();
    yield();  // cede CPU para o FreeRTOS por 1 tick
  }

  passoJammer();
}
