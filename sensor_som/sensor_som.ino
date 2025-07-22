const int pinAnalogico = A0;   // Entrada analógica
const int pinDigital = 2;      // Entrada digital
const int led = 13;            // LED embutido para testes

void setup() {
  Serial.begin(9600);          // Inicia comunicação serial
  pinMode(pinDigital, INPUT);  // D0 como entrada
  pinMode(led, OUTPUT);        // LED como saída
}

void loop() {
  int somAnalogico = analogRead(pinAnalogico); // Lê intensidade sonora
  int somDigital = digitalRead(pinDigital);    // Lê se som ultrapassou limite

  // Exibe no monitor serial
  Serial.print("Nivel Analogico: ");
  Serial.print(somAnalogico);
  Serial.print(" | Digital: ");
  Serial.println(somDigital);

  // Se som ultrapassar o limite, acende o LED
  if (somDigital == HIGH) {
    digitalWrite(led, HIGH);
  } else {
    digitalWrite(led, LOW);
  }

  delay(100); // Aguarda um pouco antes de nova leitura
}
