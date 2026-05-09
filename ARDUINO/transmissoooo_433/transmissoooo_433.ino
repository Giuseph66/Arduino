#include <RCSwitch.h>

RCSwitch tx = RCSwitch();

const byte TX_PIN = 10;
const unsigned int BITLEN = 28;
const unsigned int PULSE_US = 531;   // do seu log
const byte PROTO = 6;                // do seu log

// seus códigos
const unsigned long CODE_A = 162701861;
const unsigned long CODE_B = 162701845;

void setup() {
  Serial.begin(115200);

  tx.enableTransmit(TX_PIN);
  tx.setProtocol(PROTO);        // garante mesmo formato
  tx.setPulseLength(PULSE_US);  // ajusta o "delay(us)" capturado
  tx.setRepeatTransmit(12);     // repete o frame pra garantir alcance

  Serial.println(F("TX 433MHz pronto. Digite 'a' ou 'b' para enviar os codigos."));
  delay(1000);
}

static void sendCode(unsigned long code) {
  Serial.print(F("Enviando: ")); Serial.println(code);
  tx.send(code, BITLEN);        // envia 28 bits
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'a') sendCode(CODE_A);
    if (c == 'b') sendCode(CODE_B);
  }
}
