#include <IRremote.h>

#define IR_RECEIVE_PIN 2   // OUT do receptor → D2
#define IR_SEND_PIN    3   // LED IR com transistor (opcional)

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // pisca o LED da placa
  IrSender.begin(IR_SEND_PIN);
  Serial.println(F("Aponte um controle remoto..."));
}

void loop() {
  if (IrReceiver.decode()) {
    // Mostra resumo + como reenviar
    IrReceiver.printIRResultShort(&Serial);
    IrReceiver.printIRSendUsage(&Serial);

    // Exemplo: se for NEC ou SAMSUNG, reenvia imediatamente
    switch (IrReceiver.decodedIRData.protocol) {
      case NEC:
        IrSender.sendNEC(IrReceiver.decodedIRData.address,
                         IrReceiver.decodedIRData.command, 0);
        break;
      case SAMSUNG:
        IrSender.sendSamsung(IrReceiver.decodedIRData.address,
                             IrReceiver.decodedIRData.command, 0);
        break;
      default:
        // outros protocolos: só imprime
        break;
    }

    IrReceiver.resume(); // pronto para o próximo código
  }
}
