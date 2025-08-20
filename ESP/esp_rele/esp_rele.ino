#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

const int rele1 = 26;  // Motor 1
const int rele2 = 27;  // Motor 2

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32-Motores"); // Nome do Bluetooth

  pinMode(rele1, OUTPUT);
  pinMode(rele2, OUTPUT);

  digitalWrite(rele1, LOW); // Começa desligado
  digitalWrite(rele2, LOW);

  Serial.println("Sistema de motores iniciado. Aguardando comandos...");
}

void loop() {
  if (SerialBT.available()) {
    char comando = SerialBT.read();
    Serial.println(comando);
    switch (comando) {
      case '0':
        digitalWrite(rele1, LOW);
        digitalWrite(rele2, LOW);
        Serial.println("Todos os motores DESLIGADOS");
        break;

      case '1':
        digitalWrite(rele1, HIGH);
        Serial.println("Motor 1 LIGADO");
        break;

      case '2':
        digitalWrite(rele2, HIGH);
        Serial.println("Motor 2 LIGADO");
        break;

      case '3':
        digitalWrite(rele1, LOW);
        Serial.println("Motor 1 DESLIGADO");
        break;

      case '4':
        digitalWrite(rele2, LOW);
        Serial.println("Motor 2 DESLIGADO");
        break;

      default:
        Serial.println("Comando inválido");
        break;
    }
  }
}
