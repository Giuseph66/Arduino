#define AIA 2
#define AIB 3
#define BIA 4
#define BIB 5

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

  pinMode(AIA, OUTPUT);
  pinMode(AIB, OUTPUT);
  pinMode(BIA, OUTPUT);
  pinMode(BIB, OUTPUT);

  pinMode(sensor1DO, INPUT);
  pinMode(sensor2DO, INPUT);
}

void frente() {
  digitalWrite(AIA, HIGH);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, HIGH);
  digitalWrite(BIB, LOW);
}

void parar() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, LOW);
}

void virarEsquerda() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, HIGH);
  digitalWrite(BIB, LOW);
}

void virarDireita() {
  digitalWrite(AIA, HIGH);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, LOW);
}

void loop() {
  int s1Digital = digitalRead(sensor1DO);
  int s2Digital = digitalRead(sensor2DO);

  int s1Analogico = analogRead(sensor1AO);
  int s2Analogico = analogRead(sensor2AO);

  bool s1Preto = pretoQuandoAOMaior ? s1Analogico > limiteSensor1 : s1Analogico < limiteSensor1;
  bool s2Preto = pretoQuandoAOMaior ? s2Analogico > limiteSensor2 : s2Analogico < limiteSensor2;
  bool motor1Ligado = false;
  bool motor2Ligado = false;

  if (s1Preto && !s2Preto) {
    virarEsquerda();
    motor2Ligado = true;
  } else if (!s1Preto && s2Preto) {
    virarDireita();
    motor1Ligado = true;
  } else if (!s1Preto && !s2Preto) {
    frente();
    motor1Ligado = true;
    motor2Ligado = true;
  } else {
    parar();
  }

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
  Serial.print(s2Preto ? "PRETO" : "BRANCO");

  Serial.print(" || Motor 1/Servo 1: ");
  Serial.print(motor1Ligado ? "GIRANDO" : "PARADO");
  Serial.print(" | Motor 2/Servo 2: ");
  Serial.println(motor2Ligado ? "GIRANDO" : "PARADO");

  delay(intervaloLeituraMs);
}
