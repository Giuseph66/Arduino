// ============================================================
// smoochiee - Bluetooth Jammer ESP32 (original / mais antigo)
// Fonte: https://github.com/smoochiee/Ble-jammer
// Arquivado em nov/2024 - versão mais nova: Noisy-boy
// Hardware: ESP32 + 2x nRF24L01
// ============================================================

#include "RF24.h"
#include <SPI.h>
#include <ezButton.h>
#include "esp_bt.h"
#include "esp_wifi.h"

SPIClass *sp = nullptr;
SPIClass *hp = nullptr;

// HSPI: SCK=14, MISO=12, MOSI=13, CS=15, CE=16
RF24 radio(16, 15, 16000000);

// VSPI: SCK=18, MISO=19, MOSI=23, CS=21, CE=22
RF24 radio1(22, 21, 16000000);

unsigned int flag  = 0;  // HSPI direction flag
unsigned int flagv = 0;  // VSPI direction flag
int ch  = 45;            // HSPI channel
int ch1 = 45;            // VSPI channel

ezButton toggleSwitch(33);

// ------------------------------------------------------------
// Modo "two": hopping sequencial
//   HSPI: passo 2  (bounce 2-79)
//   VSPI: passo 4  (bounce 2-79)
// ------------------------------------------------------------
void two() {
  if (flagv == 0) {
    ch1 += 4;
  } else {
    ch1 -= 4;
  }
  if (flag == 0) {
    ch += 2;
  } else {
    ch -= 2;
  }

  if ((ch1 > 79) && (flagv == 0)) flagv = 1;
  else if ((ch1 < 2) && (flagv == 1)) flagv = 0;

  if ((ch > 79) && (flag == 0)) flag = 1;
  else if ((ch < 2) && (flag == 1)) flag = 0;

  radio.setChannel(ch);
  radio1.setChannel(ch1);
}

// ------------------------------------------------------------
// Modo "one": canal ALEATÓRIO nos dois rádios
// Comentário original indica que sweep sequencial também é opção
// ------------------------------------------------------------
void one() {
  radio1.setChannel(random(80));
  radio.setChannel(random(80));
  delayMicroseconds(random(60));  // remover se ficar lento

  // Alternativa de sweep sequencial (comentada no original):
  // for (int i = 0; i < 79; i++) {
  //   radio.setChannel(i);
  // }
}

void initSP() {
  sp = new SPIClass(VSPI);
  sp->begin();
  if (radio1.begin(sp)) {
    Serial.println("SP Started !!!");
    radio1.setAutoAck(false);
    radio1.stopListening();
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MAX, true);
    radio1.setDataRate(RF24_2MBPS);
    radio1.setCRCLength(RF24_CRC_DISABLED);
    radio1.printPrettyDetails();
    radio1.startConstCarrier(RF24_PA_MAX, ch1);
  } else {
    Serial.println("SP couldn't start !!!");
  }
}

void initHP() {
  hp = new SPIClass(HSPI);
  hp->begin();
  if (radio.begin(hp)) {
    Serial.println("HP Started !!!");
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.printPrettyDetails();
    radio.startConstCarrier(RF24_PA_MAX, ch);
  } else {
    Serial.println("HP couldn't start !!!");
  }
}

void setup() {
  Serial.begin(115200);
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();
  toggleSwitch.setDebounceTime(50);
  initHP();
  initSP();
}

void loop() {
  toggleSwitch.loop();  // OBRIGATÓRIO chamar antes de getState()
  int state = toggleSwitch.getState();
  if (state == HIGH)
    two();  // hopping sequencial
  else
    one();  // canal aleatório
}
