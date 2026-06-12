// Motor 1: AIA=2, AIB=3 | Motor 2: BIA=4, BIB=5 | Sensores: AO em A0/A1

#define AIA 2
#define AIB 3
#define BIA 4
#define BIB 5

const int sensor1DO = 6;
const int sensor2DO = 7;
const int sensor1AO = A0;
const int sensor2AO = A1;

const int velocidadeReta  = 60;
const int velocidadeCurva = 150;

const bool SENSOR1_E_O_DA_DIREITA = false;

const bool MOTOR1_INVERTIDO = false;
const bool MOTOR2_INVERTIDO = true;

// Compensação de força por motor, em % (100 = sem ajuste)
const int ganhoMotor1Pct = 100;
const int ganhoMotor2Pct = 115;

// Branco: S1 lê ~11-58, S2 lê ~11-24. Preto sobe acima de 50/70.
const int limitePretoS1 = 70;
const int limitePretoS2 = 50;
const int histerese = 20;

const unsigned long intervaloSerialMs = 200;

void motor1(int velocidade) {
  if (MOTOR1_INVERTIDO) velocidade = -velocidade;
  velocidade = (int)((long)velocidade * ganhoMotor1Pct / 100);
  velocidade = constrain(velocidade, -255, 255);
  if (velocidade > 0) {
    digitalWrite(AIA, HIGH);
    analogWrite(AIB, 255 - velocidade);
  } else if (velocidade < 0) {
    digitalWrite(AIA, LOW);
    analogWrite(AIB, -velocidade);
  } else {
    digitalWrite(AIA, LOW);
    analogWrite(AIB, 0);
  }
}

void motor2(int velocidade) {
  if (MOTOR2_INVERTIDO) velocidade = -velocidade;
  velocidade = (int)((long)velocidade * ganhoMotor2Pct / 100);
  velocidade = constrain(velocidade, -255, 255);
  if (velocidade > 0) {
    digitalWrite(BIA, HIGH);
    analogWrite(BIB, 255 - velocidade);
  } else if (velocidade < 0) {
    digitalWrite(BIA, LOW);
    analogWrite(BIB, -velocidade);
  } else {
    digitalWrite(BIA, LOW);
    analogWrite(BIB, 0);
  }
}

void rodas(int esquerda, int direita) {
  motor1(direita);
  motor2(esquerda);
}

void frente(int v)       { rodas(v, v); }
void pivoDireita(int v)  { rodas(v, -v); }
void pivoEsquerda(int v) { rodas(-v, v); }
void parar()             { rodas(0, 0); }

bool lerPreto(int pinoAO, int limite, bool &estavaPreto) {
  int a = analogRead(pinoAO);
  if (estavaPreto) {
    if (a < limite - histerese) estavaPreto = false;
  } else {
    if (a > limite) estavaPreto = true;
  }
  return estavaPreto;
}

void setup() {
  Serial.begin(115200);

  pinMode(AIA, OUTPUT);
  pinMode(AIB, OUTPUT);
  pinMode(BIA, OUTPUT);
  pinMode(BIB, OUTPUT);

  pinMode(sensor1DO, INPUT);
  pinMode(sensor2DO, INPUT);

  parar();
}

void loop() {
  static bool s1Estado = false;
  static bool s2Estado = false;
  bool s1Preto = lerPreto(sensor1AO, limitePretoS1, s1Estado);
  bool s2Preto = lerPreto(sensor2AO, limitePretoS2, s2Estado);

  bool direitaPreto  = SENSOR1_E_O_DA_DIREITA ? s1Preto : s2Preto;
  bool esquerdaPreto = SENSOR1_E_O_DA_DIREITA ? s2Preto : s1Preto;

  unsigned long agora = millis();
  const char *acao;

  static int ultimaAcao = 0; // 0 = frente, 1 = direita, 2 = esquerda

  if (direitaPreto && !esquerdaPreto) {
    pivoDireita(velocidadeCurva);
    ultimaAcao = 1;
    acao = "CURVA DIREITA";
  } else if (esquerdaPreto && !direitaPreto) {
    pivoEsquerda(velocidadeCurva);
    ultimaAcao = 2;
    acao = "CURVA ESQUERDA";
  } else if (esquerdaPreto && direitaPreto) {
    if (ultimaAcao == 1) {
      pivoDireita(velocidadeCurva);
      acao = "CONTINUA DIREITA";
    } else if (ultimaAcao == 2) {
      pivoEsquerda(velocidadeCurva);
      acao = "CONTINUA ESQUERDA";
    } else {
      frente(velocidadeReta);
      acao = "CRUZAMENTO";
    }
  } else {
    frente(velocidadeReta);
    ultimaAcao = 0;
    acao = "FRENTE";
  }

  static unsigned long ultimaImpressao = 0;
  if (agora - ultimaImpressao >= intervaloSerialMs) {
    ultimaImpressao = agora;
    Serial.print("S1(AO=");
    Serial.print(analogRead(sensor1AO));
    Serial.print(")=");
    Serial.print(s1Preto ? "PRETO " : "BRANCO");
    Serial.print(" | S2(AO=");
    Serial.print(analogRead(sensor2AO));
    Serial.print(")=");
    Serial.print(s2Preto ? "PRETO " : "BRANCO");
    Serial.print(" | ");
    Serial.println(acao);
  }
}
