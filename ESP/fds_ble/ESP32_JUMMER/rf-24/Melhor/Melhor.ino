// ============================================================
//  Melhor.ino — Jammer 2.4 GHz (ESP32 + 2x nRF24L01+ PA/LNA)
//  Autor: Jesus (combinação otimizada de múltiplos projetos)
//  Baseado em: smoochiee, SpiritBox, rf-24-c2 (projeto próprio)
//
//  O QUE ESSE CÓDIGO FAZ:
//    Utiliza dois módulos nRF24L01+ PA/LNA em barramentos SPI
//    diferentes do ESP32 (HSPI e VSPI) para emitir portadora
//    constante e varrer todo o espectro 2.4 GHz (canais 0-79),
//    cobrindo Bluetooth clássico, BLE, Wi-Fi 2.4 GHz e drones RC.
//
//  MODOS DE OPERAÇÃO (automáticos, alterna a cada ciclo):
//    1. Varredura sequencial — percorre canais 0-79 sem delay
//    2. Hopping aleatório  — canais aleatórios com micro-delay
//    3. Hopping em passo   — rádios com passos diferentes (cobertura assimétrica)
//
//  PINOS UTILIZADOS (mesmos do seu projeto rf-24-c2):
//    HSPI — SCK=14, MISO=12, MOSI=13, CSN=15, CE=16
//    VSPI — SCK=18, MISO=19, MOSI=23, CSN=21, CE=22
//
//  CAPACITOR: 100 µF eletrolítico entre VCC e GND de cada módulo
//  SPI SPEED: 16 MHz (aumentado de 4 MHz — 4x mais rápido na troca de canal)
// ============================================================

#include <SPI.h>
#include "RF24.h"
#include "esp_bt.h"
#include "esp_wifi.h"

// ------------------------------------------------------------
// Configurações ajustáveis
// ------------------------------------------------------------

// Faixa de canais do espectro 2.4 GHz
// Canal 0  = 2400 MHz  (início do BLE — advertising ch 37)
// Canal 12 = 2412 MHz  (advertising BLE ch 38)
// Canal 39 = 2439 MHz  (advertising BLE ch 39)
// Canal 79 = 2479 MHz  (fim do Bluetooth clássico)
#define CANAL_MIN   0    // começa no início real do espectro BT/BLE
#define CANAL_MAX   79   // fim do BT clássico (80 canais no total)

// Velocidade de comunicação SPI com o nRF24 (16 MHz = máximo estável)
#define SPI_VELOCIDADE  16000000

// Quantos ciclos de varredura sequencial antes de trocar de modo
#define CICLOS_SWEEP    3

// Quantos ciclos de hopping aleatório antes de trocar de modo
#define CICLOS_RANDOM   3

// ------------------------------------------------------------
// Barramento SPI e rádios
// ------------------------------------------------------------

// Ponteiros para os barramentos SPI (alocados dinamicamente no setup)
SPIClass *hspi = nullptr;
SPIClass *vspi = nullptr;

// Radio 1 no barramento HSPI: CE=16, CSN=15
// HSPI pinos: SCK=14, MISO=12, MOSI=13
RF24 radio1(16, 15, SPI_VELOCIDADE);

// Radio 2 no barramento VSPI: CE=22, CSN=21
// VSPI pinos: SCK=18, MISO=19, MOSI=23
RF24 radio2(22, 21, SPI_VELOCIDADE);

// ------------------------------------------------------------
// Variáveis de controle de hopping
// ------------------------------------------------------------

// Canais atuais de cada rádio
uint8_t canal1 = CANAL_MIN;
uint8_t canal2 = CANAL_MIN + 1;  // rádio 2 começa defasado (cobre canais diferentes)

// Direção do bounce (0 = subindo, 1 = descendo)
uint8_t direcao1 = 0;
uint8_t direcao2 = 0;

// Contador de modo atual e ciclos
uint8_t modo       = 0;
uint8_t contCiclos = 0;

// ------------------------------------------------------------
// Inicialização de um rádio nRF24L01+
// Parâmetros configurados para máxima interferência:
//   - AutoAck desligado     → sem retransmissões
//   - Retries zerados       → sem espera entre tentativas
//   - Potência máxima       → RF24_PA_MAX
//   - Taxa 2 Mbps           → largura de banda máxima do chip
//   - CRC desligado         → sem overhead de verificação
//   - startConstCarrier     → emite portadora RF contínua no canal
// ------------------------------------------------------------
bool inicializarRadio(RF24 &radio, SPIClass *spiBus, uint8_t canalInicial, const char *nome) {
  if (!radio.begin(spiBus)) {
    Serial.print(nome);
    Serial.println(": FALHOU ao iniciar! Verifique alimentação, GND e pinos SPI.");
    return false;
  }

  delay(100);  // aguarda estabilização após init

  // Configura para jamming — desliga tudo que gera overhead
  radio.setAutoAck(false);          // sem ACK automático
  radio.stopListening();            // modo transmissão
  radio.setRetries(0, 0);           // sem retentativas
  radio.setPayloadSize(5);          // payload mínimo (modificado na lib)
  radio.setAddressWidth(3);         // endereço mínimo (modificado na lib)
  radio.setPALevel(RF24_PA_MAX, true); // potência máxima + LNA ativo
  radio.setDataRate(RF24_2MBPS);    // taxa máxima = maior largura de banda RF
  radio.setCRCLength(RF24_CRC_DISABLED); // sem CRC = sem overhead

  // Imprime detalhes no Serial para diagnóstico
  Serial.print(nome);
  Serial.println(": OK — configuração:");
  radio.printPrettyDetails();

  // Inicia portadora constante no canal inicial
  // IMPORTANTE: startConstCarrier mantém o chip emitindo RF continuamente.
  // A chamada setChannel() posterior muda o canal sem parar a portadora.
  radio.startConstCarrier(RF24_PA_MAX, canalInicial);

  Serial.print(nome);
  Serial.print(": portadora constante iniciada no canal ");
  Serial.println(canalInicial);

  return true;
}

// ------------------------------------------------------------
// MODO 1 — Varredura sequencial rápida (sweep)
// Os dois rádios percorrem os canais 0-79 de forma complementar:
//   Radio1: canais pares   (0, 2, 4 ... 78)
//   Radio2: canais ímpares (1, 3, 5 ... 79)
// Isso garante cobertura de TODO o espectro a cada ciclo,
// com os dois rádios trabalhando em paralelo sem sobreposição.
// ------------------------------------------------------------
void modoSweep() {
  for (uint8_t ch = CANAL_MIN; ch <= CANAL_MAX; ch++) {
    // Radio1 cobre canais pares, radio2 cobre ímpares
    // Ambos são setados a cada iteração para máxima frequência
    if (ch % 2 == 0) {
      radio1.setChannel(ch);
    } else {
      radio2.setChannel(ch);
    }
    // Sem delay — velocidade máxima de varredura
  }
}

// ------------------------------------------------------------
// MODO 2 — Hopping aleatório
// Cada rádio salta para um canal completamente aleatório.
// Mais eficaz contra dispositivos com FHSS (Frequency Hopping
// Spread Spectrum), pois o padrão de interferência é imprevisível.
// Um micro-delay aleatório simula variação de tempo real.
// ------------------------------------------------------------
void modoAleatorio() {
  radio1.setChannel(random(CANAL_MIN, CANAL_MAX + 1));
  radio2.setChannel(random(CANAL_MIN, CANAL_MAX + 1));
  delayMicroseconds(random(10, 80));  // micro-delay aleatório
}

// ------------------------------------------------------------
// MODO 3 — Hopping em passos assimétricos (bounce)
// Radio1: passo +2 (canais de 2 em 2, bounce nos extremos)
// Radio2: passo +4 (canais de 4 em 4, bounce nos extremos)
// Os dois rádios se movem em velocidades diferentes, garantindo
// que em qualquer instante estejam cobrindo regiões distintas
// do espectro — sem se sobrepor no mesmo canal ao mesmo tempo.
// ------------------------------------------------------------
void modoHopping() {
  // Atualiza canal do rádio 1 (passo 2)
  if (direcao1 == 0) {
    canal1 += 2;
    if (canal1 >= CANAL_MAX) direcao1 = 1;
  } else {
    if (canal1 <= CANAL_MIN + 2) direcao1 = 0;
    else canal1 -= 2;
  }

  // Atualiza canal do rádio 2 (passo 4)
  if (direcao2 == 0) {
    canal2 += 4;
    if (canal2 >= CANAL_MAX) direcao2 = 1;
  } else {
    if (canal2 <= CANAL_MIN + 4) direcao2 = 0;
    else canal2 -= 4;
  }

  radio1.setChannel(canal1);
  radio2.setChannel(canal2);
}

// ------------------------------------------------------------
// Setup — executado uma vez ao ligar
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  Melhor.ino — Jammer 2.4 GHz");
  Serial.println("  ESP32 + 2x nRF24L01+ PA/LNA");
  Serial.println("============================================");

  // Desliga completamente o Bluetooth e Wi-Fi internos do ESP32.
  // Isso evita interferência do próprio ESP32 no espectro 2.4 GHz
  // e libera energia do regulador para os módulos nRF24.
  Serial.println("Desligando BT e Wi-Fi internos do ESP32...");
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();
  Serial.println("BT e Wi-Fi desligados.");

  // Inicializa o barramento HSPI com os pinos padrão do ESP32
  // SCK=14, MISO=12, MOSI=13 (CSN e CE são passados ao RF24)
  Serial.println("\nInicializando HSPI (Radio 1)...");
  hspi = new SPIClass(HSPI);
  hspi->begin();  // usa pinos padrão HSPI do ESP32

  // Inicializa o barramento VSPI com os pinos padrão do ESP32
  // SCK=18, MISO=19, MOSI=23 (CSN e CE são passados ao RF24)
  Serial.println("Inicializando VSPI (Radio 2)...");
  vspi = new SPIClass(VSPI);
  vspi->begin();  // usa pinos padrão VSPI do ESP32

  delay(100);

  // Inicializa os dois rádios
  bool ok1 = inicializarRadio(radio1, hspi, CANAL_MIN,     "Radio1 (HSPI)");
  delay(200);
  bool ok2 = inicializarRadio(radio2, vspi, CANAL_MIN + 1, "Radio2 (VSPI)");

  // Se algum rádio não iniciar, fica em loop piscando no Serial
  if (!ok1 || !ok2) {
    Serial.println("\n!!! ERRO: Um ou mais radios falharam !!!");
    Serial.println("Verifique:");
    Serial.println("  1. Alimentacao 3.3V estavel (capacitor 100uF em cada modulo)");
    Serial.println("  2. GND comum entre ESP32 e modulos");
    Serial.println("  3. Pinos SPI corretos (SCK, MISO, MOSI, CSN, CE)");
    Serial.println("  4. Modulo e nRF24L01+ PA/LNA (com antena)");
    while (true) delay(1000);
  }

  Serial.println("\n============================================");
  Serial.println("  Ambos os radios OK! Iniciando jamming...");
  Serial.println("  Modos: Sweep -> Aleatorio -> Hopping");
  Serial.println("============================================\n");
}

// ------------------------------------------------------------
// Loop principal — alterna entre os 3 modos automaticamente
// ------------------------------------------------------------
void loop() {
  switch (modo) {

    case 0:
      // Modo sweep sequencial — cobre 0-79 dividido entre os 2 rádios
      modoSweep();
      contCiclos++;
      if (contCiclos >= CICLOS_SWEEP) {
        contCiclos = 0;
        modo = 1;
        Serial.println("[modo] Aleatorio");
      }
      break;

    case 1:
      // Modo aleatório — eficaz contra FHSS
      modoAleatorio();
      contCiclos++;
      if (contCiclos >= (CICLOS_RANDOM * 500)) {  // ~500 iterações por "ciclo"
        contCiclos = 0;
        modo = 2;
        Serial.println("[modo] Hopping em passos");
      }
      break;

    case 2:
      // Modo hopping assimétrico
      modoHopping();
      contCiclos++;
      if (contCiclos >= (CICLOS_SWEEP * 40)) {  // ~40 iterações por "ciclo"
        contCiclos = 0;
        modo = 0;
        Serial.println("[modo] Sweep sequencial");
      }
      break;

    default:
      modo = 0;
      break;
  }
}
