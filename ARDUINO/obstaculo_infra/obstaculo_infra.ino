const int sensor1DO = 6;
const int sensor2DO = 7;

const int sensor1AO = A0;
const int sensor2AO = A1;

const int intervaloLeituraMs = 10;

const int limiteSensor1 = 100;
const int limiteSensor2 = 100;
const bool pretoQuandoAOMaior = true;

void setup() {
  Serial.begin(115200);

  pinMode(sensor1DO, INPUT);
  pinMode(sensor2DO, INPUT);
}

void loop() {
  int s1Digital = digitalRead(sensor1DO);
  int s2Digital = digitalRead(sensor2DO);

  int s1Analogico = analogRead(sensor1AO);
  int s2Analogico = analogRead(sensor2AO);

  bool s1Preto = pretoQuandoAOMaior ? s1Analogico > limiteSensor1 : s1Analogico < limiteSensor1;
  bool s2Preto = pretoQuandoAOMaior ? s2Analogico > limiteSensor2 : s2Analogico < limiteSensor2;

  Serial.print("Sensor 1 DO: ");
  Serial.print(s1Digital);
  Serial.print(" | AO: ");
  Serial.print(s1Analogico);
  Serial.print(" | COR: ");
  Serial.print(s1Preto ? "PRETO" : "BRANCO");

  Serial.print(" || Sensor 2 DO: ");
  Serial.print(s2Digital);
  Serial.print(" | AO: ");
  Serial.print(s2Analogico);
  Serial.print(" | COR: ");
  Serial.println(s2Preto ? "PRETO" : "BRANCO");

  delay(intervaloLeituraMs);
}
