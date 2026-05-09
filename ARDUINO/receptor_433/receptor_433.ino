#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

void setup() {
  Serial.begin(115200);
  mySwitch.enableReceive(digitalPinToInterrupt(2)); // Receptor no D2 (INT0)
  Serial.println(F("\n=== RCSwitch Receiver (433MHz) ==="));
}

void loop() {
  if (mySwitch.available()) {
    unsigned long value = mySwitch.getReceivedValue();
    unsigned int bitlen  = mySwitch.getReceivedBitlength();
    unsigned int delayUs = mySwitch.getReceivedDelay();
    unsigned int proto   = mySwitch.getReceivedProtocol();

    if (value == 0) {
      Serial.println(F("Codigo invalido/rolling ou desconhecido."));
    } else {
      Serial.print(F("Codigo: ")); Serial.print(value);
      Serial.print(F("  Bits: ")); Serial.print(bitlen);
      Serial.print(F("  Delay(us): ")); Serial.print(delayUs);
      Serial.print(F("  Protocolo: ")); Serial.println(proto);
    }
    mySwitch.resetAvailable();
  }
}