#include "RF24.h"

#include "esp_bt.h"
#include "esp_wifi.h"

SPIClass *hspi = nullptr;
SPIClass *vspi = nullptr;

//HSPI=SCK = 14, MISO = 12, MOSI = 13, CS = 15 , CE = 16
//VSPI=SCK = 18, MISO =19, MOSI = 23 ,CS =21 ,CE = 22

// Radio 1: HSPI (CE=16, CS=15)
RF24 radio1(16, 15, 4000000);

// Radio 2: VSPI (CE=22, CS=21)
RF24 radio2(22, 21, 4000000);

byte channel1 = 14;  ///Radio1: pares (14,16,18...)
byte channel2 = 15;  ///Radio2: ímpares (15,17,19...)

unsigned int flag1 = 0;
unsigned int flag2 = 0;



void initRadio1_HSPI() {
  hspi = new SPIClass(HSPI);
  hspi->begin();
  if (radio1.begin(hspi)) {
    delay(200);
    Serial.println("Radio1 (HSPI) Started !!!");
    radio1.setAutoAck(false);
    radio1.stopListening();
    radio1.setRetries(0, 0);
    radio1.setPayloadSize(5);
    radio1.setAddressWidth(3);
    radio1.setPALevel(RF24_PA_MAX, true);
    radio1.setDataRate(RF24_2MBPS);
    radio1.setCRCLength(RF24_CRC_DISABLED);
    radio1.printPrettyDetails();
    radio1.startConstCarrier(RF24_PA_MAX, channel1);
  } else {
    Serial.println("Radio1 (HSPI) couldn't start !!!");
  }
}

void initRadio2_VSPI() {
  vspi = new SPIClass(VSPI);
  vspi->begin();
  if (radio2.begin(vspi)) {
    delay(200);
    Serial.println("Radio2 (VSPI) Started !!!");
    radio2.setAutoAck(false);
    radio2.stopListening();
    radio2.setRetries(0, 0);
    radio2.setPayloadSize(5);
    radio2.setAddressWidth(3);
    radio2.setPALevel(RF24_PA_MAX, true);
    radio2.setDataRate(RF24_2MBPS);
    radio2.setCRCLength(RF24_CRC_DISABLED);
    radio2.printPrettyDetails();
    radio2.startConstCarrier(RF24_PA_MAX, channel2);
  } else {
    Serial.println("Radio2 (VSPI) couldn't start !!!");
  }
}
void hopChannel_Radio1() {
  if (flag1 == 0) {
    if (channel1 >= 78) {
      flag1 = 1;
    } else {
      channel1 += 2;
    }
  } else {
    if (channel1 <= 14) {
      flag1 = 0;
    } else {
      channel1 -= 2;
    }
  }
  radio1.setChannel(channel1);
}

void hopChannel_Radio2() {
  if (flag2 == 0) {
    if (channel2 >= 79) {
      flag2 = 1;
    } else {
      channel2 += 2;
    }
  } else {
    if (channel2 <= 15) {
      flag2 = 0;
    } else {
      channel2 -= 2;
    }
  }
  radio2.setChannel(channel2);
}

void sweepChannel_Radio1() {
  for (int ch = 14; ch <= 79; ch += 2) {  // 14,16,18...78 (pares)
    radio1.setChannel(ch);
  }
}

void sweepChannel_Radio2() {
  for (int ch = 15; ch <= 79; ch += 2) {  // 15,17,19...79 (ímpares)
    radio2.setChannel(ch);
  }
}

void fullSweep_Both() {
  sweepChannel_Radio1();
  sweepChannel_Radio2();
}




void setup(void) {
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  Serial.begin(115200);

  Serial.println("\n=== Initializing Dual RF24 ===");
  initRadio1_HSPI();
  delay(500);
  initRadio2_VSPI();
  Serial.println("=== Both Radios Ready ===\n");
}

void loop(void) {
  // Sweep completo: percorre 14..79 sem delay (como original)
  fullSweep_Both();

  // Alternativa: usar hopping com 100ms delay
  // hopChannel_Radio1();
  // hopChannel_Radio2();
  // delay(100);
}
