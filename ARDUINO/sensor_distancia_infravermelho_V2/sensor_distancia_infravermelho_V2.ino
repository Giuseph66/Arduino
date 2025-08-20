const int pinoOUT = 2;   // Saída do sensor (OUT)
const int pinoEN = 3;    // Pino de Enable do sensor
const int led = 13;

void setup() {
  pinMode(pinoOUT, INPUT);
  pinMode(pinoEN, OUTPUT);
  pinMode(led, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  // Ativa o sensor momentaneamente
  digitalWrite(pinoEN, HIGH); // Liga o sensor
  delay(100); // Tempo para estabilizar e ler
  int leitura = digitalRead(pinoOUT);

  if (leitura == LOW) {
    Serial.println("Obstáculo detectado!");
    digitalWrite(led, HIGH);
  } else {
    Serial.println("Nenhum obstáculo.");
    digitalWrite(led, LOW);
  }

  digitalWrite(pinoEN, LOW); // Desliga o sensor para evitar leituras falsas
  delay(500); // Aguarda antes da próxima leitura
}
