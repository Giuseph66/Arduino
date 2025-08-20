const int sensorIR = 2;  // Pino OUT conectado ao D2
const int led = 13;      // LED embutido para teste

void setup() {
  pinMode(sensorIR, INPUT);
  pinMode(led, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  int estado = digitalRead(sensorIR);
  Serial.println(estado);
  if (estado == LOW) { // alguns sensores mandam LOW quando detectam
    Serial.println("Obstáculo detectado!");
    digitalWrite(led, HIGH);
  } else {
    Serial.println("Livre");
    digitalWrite(led, LOW);
  }

  delay(100);
}
