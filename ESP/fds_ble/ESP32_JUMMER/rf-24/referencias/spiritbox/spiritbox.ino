// ============================================================
// SpiritBox - by rubberpirate
// Fonte: https://github.com/rubberpirate/SpiritBox
// ESP32 + 2x nRF24L01 (HSPI + VSPI) + OLED SSD1306 + toggle switch
// ============================================================

#include "RF24.h"
#include <SPI.h>
#include <Wire.h>
#include "esp_bt.h"
#include "esp_wifi.h"

/* OLED SDA D21  SCL D22 */
#include <Arduino.h>
#include <U8x8lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

U8X8_SSD1306_128X32_UNIVISION_SW_I2C u8x8(SCL, SDA, U8X8_PIN_NONE);

SPIClass *sp = nullptr;
SPIClass *hp = nullptr;

// NRF24-1 HSPI: SCK=14, MISO=12, MOSI=13, CS=15, CE=26
RF24 radio(26, 15, 16000000);

// NRF24-2 VSPI: SCK=18, MISO=19, MOSI=23, CS=2, CE=4
RF24 radio1(4, 2, 16000000);

unsigned int flag  = 0;  // HSPI direction flag
unsigned int flagv = 0;  // VSPI direction flag
int ch  = 45;            // HSPI channel
int ch1 = 45;            // VSPI channel

#include <ezButton.h>
ezButton toggleSwitch(33);

// ------------------------------------------------------------
// Modo "two": hopping com passo 2 (HSPI) e passo 4 (VSPI),
// bounce entre 2 e 79
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
// com microssegundos de delay aleatório
// ------------------------------------------------------------
void one() {
  radio1.setChannel(random(80));
  radio.setChannel(random(80));
  delayMicroseconds(random(60));  // remover se ficar lento
}

void initSP() {
  sp = new SPIClass(VSPI);
  sp->begin();
  if (radio1.begin(sp)) {
    Serial.println("VSPI Jammer Started !!!");
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0, 0, " J1 Firing ^_^");
    u8x8.refreshDisplay();
    radio1.setAutoAck(false);
    radio1.stopListening();
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MAX, true);
    radio1.setDataRate(RF24_2MBPS);
    radio1.setCRCLength(RF24_CRC_DISABLED);
    radio1.printPrettyDetails();
    radio1.startConstCarrier(RF24_PA_MAX, ch1);
  } else {
    Serial.println("VSPI Jammer couldn't start !!!");
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0, 0, " J1 Died *_*");
    u8x8.refreshDisplay();
  }
}

void initHP() {
  hp = new SPIClass(HSPI);
  hp->begin();
  if (radio.begin(hp)) {
    Serial.println("HSPI Started !!!");
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0, 1, " J2 Firing ^_^");
    u8x8.refreshDisplay();
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.printPrettyDetails();
    radio.startConstCarrier(RF24_PA_MAX, ch);
  } else {
    Serial.println("HSPI couldn't start !!!");
    u8x8.clear();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0, 1, " J2 Died *_*");
    u8x8.refreshDisplay();
  }
}

void setup() {
  Serial.begin(115200);

  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 0, " Spirit Box");
  u8x8.drawString(0, 1, " -----------");
  u8x8.drawString(0, 3, " Rubber Pirate");
  u8x8.refreshDisplay();
  delay(5000);

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
