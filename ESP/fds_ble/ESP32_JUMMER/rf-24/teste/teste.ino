#include <SPI.h>
#include <RF24.h>

// Cada RF24 usa um barramento SPI do ESP32.
SPIClass hspi(HSPI);
SPIClass vspi(VSPI);

// Modulo A no VSPI: SCK 18, MISO 19, MOSI 23, CE 22, CSN 21
// Modulo B no HSPI: SCK 14, MISO 12, MOSI 13, CE 16, CSN 15
RF24 radioA(22, 21, 4000000);
RF24 radioB(16, 15, 4000000);

const byte addressA[6] = "RFAAA";
const byte addressB[6] = "RFBBB";

struct TestPacket {
  uint32_t sequence;
  char text[16];
};

const uint8_t testPayloadSize = sizeof(TestPacket);

bool configureRadio(RF24& radio, SPIClass* spiBus,
                    const byte* ownAddress, const byte* peerAddress,
                    const char* name) {
  if (!radio.begin(spiBus)) {
    Serial.print(name);
    Serial.println(": falha no radio.begin()");
    return false;
  }

  if (!radio.isChipConnected()) {
    Serial.print(name);
    Serial.println(": chip nao conectado");
    return false;
  }

  radio.setChannel(76);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAddressWidth(5);
  radio.setPayloadSize(testPayloadSize);
  radio.setAutoAck(true);
  radio.setRetries(5, 15);
  radio.flush_tx();
  radio.flush_rx();

  // Cada modulo recebe no proprio endereco e transmite para o outro.
  radio.openWritingPipe(peerAddress);
  radio.openReadingPipe(1, ownAddress);

  Serial.print(name);
  Serial.println(": SPI e chip OK");
  return true;
}

bool receivePacket(RF24& radio, const char* name, uint32_t expectedSequence) {
  bool expectedPacketReceived = false;
  const uint32_t timeout = millis() + 100;

  while (millis() < timeout) {
    while (radio.available()) {
      TestPacket packet{};
      radio.read(&packet, sizeof(packet));

      Serial.print(name);
      Serial.print(" recebeu pacote ");
      Serial.print(packet.sequence);
      Serial.print(" - ");
      Serial.println(packet.text);

      if (packet.sequence == expectedSequence) {
        expectedPacketReceived = true;
      }
    }

    if (expectedPacketReceived) {
      break;
    }
    delay(1);
  }

  return expectedPacketReceived;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  vspi.begin(18, 19, 23, 21);
  hspi.begin(14, 12, 13, 15);

  const bool radioAOk = configureRadio(
      radioA, &vspi, addressA, addressB, "Radio A");
  const bool radioBOk = configureRadio(
      radioB, &hspi, addressB, addressA, "Radio B");

  if (!radioAOk || !radioBOk) {
    Serial.println("Verifique alimentacao, GND, SPI, CE e CSN.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Teste bidirecional iniciado");
}

void loop() {
  static uint32_t sequence = 1;

  // A -> B
  radioA.stopListening();
  radioA.flush_tx();
  radioB.stopListening();
  radioB.flush_rx();
  radioB.startListening();
  delay(10);

  TestPacket packetA = {sequence, "A para B"};
  const bool sentA = radioA.write(&packetA, sizeof(packetA));
  const bool receivedB = receivePacket(radioB, "Radio B", sequence);

  Serial.print("A -> B: ");
  if (receivedB && sentA) {
    Serial.println("OK");
  } else if (receivedB) {
    Serial.println("RECEBIDO (sem ACK)");
  } else {
    Serial.println("FALHA");
  }

  // B -> A
  radioB.stopListening();
  radioB.flush_tx();
  radioA.stopListening();
  radioA.flush_rx();
  radioA.startListening();
  delay(10);

  TestPacket packetB = {sequence, "B para A"};
  const bool sentB = radioB.write(&packetB, sizeof(packetB));
  const bool receivedA = receivePacket(radioA, "Radio A", sequence);

  Serial.print("B -> A: ");
  if (receivedA && sentB) {
    Serial.println("OK");
  } else if (receivedA) {
    Serial.println("RECEBIDO (sem ACK)");
  } else {
    Serial.println("FALHA");
  }
  Serial.println();

  radioB.startListening();
  sequence++;
  delay(1000);
}
