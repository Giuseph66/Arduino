// Carrinho segue-linha — 2 sensores IR + ponte H (L9110/similar) + Arduino Nano
//
// PORTAS FIXAS (não alterar):
//   Motor 1: AIA=2 (direção), AIB=3 (PWM)
//   Motor 2: BIA=4 (direção), BIB=5 (PWM)
//   Sensores: DO 6/7, AO A0/A1
//
// COMO AJUSTAR:
//   1. Abra o Serial Monitor a 115200 e passe os sensores sobre a linha.
//      Se PRETO/BRANCO aparecer invertido, troque pretoQuandoDOBaixo.
//   2. Se o carrinho virar para o lado errado, troque SENSOR1_E_O_DA_DIREITA.
//   3. Se ainda passar reto nas curvas, aumente velocidadeCurva (até 255)
//      ou diminua velocidadeReta.

#define AIA 2
#define AIB 3
#define BIA 4
#define BIB 5

const int sensor1DO = 6;
const int sensor2DO = 7;
const int sensor1AO = A0;
const int sensor2AO = A1;

// ---------------- Ajustes principais ----------------
const int velocidadeReta  = 60;  // reta: baixo o bastante p/ dar tempo de reagir
const int velocidadeCurva = 150;  // pivô precisa de PWM alto, senão o motor trava

// Sensor 1 fica do lado DIREITO do carrinho? (true = sim)
// Sensor da direita vê preto -> linha fugiu p/ direita -> virar p/ direita.
const bool SENSOR1_E_O_DA_DIREITA = false;

// Se uma roda girar ao contrário no "frente", inverta o motor correspondente aqui.
const bool MOTOR1_INVERTIDO = false;
const bool MOTOR2_INVERTIDO = true;

// Compensação de força por motor, em % (100 = sem ajuste).
// Motor 2 sai mais fraco (inversão usa fast decay + variação entre motores).
const int ganhoMotor1Pct = 100;
const int ganhoMotor2Pct = 115;

// Leitura do sensor (analógica; AO maior = preto)
// Limite por sensor: S1 (D6/A0) tem sinal mais fraco/lento — limite mais baixo
// para disparar mais cedo. Branco: S1 lê ~11-58, S2 lê ~11-24.
const int limitePretoS1 = 70;
const int limitePretoS2 = 50;
// Histerese: depois de entrar no preto, só volta a branco abaixo de
// (limite - histerese). Evita tremer na transição.
const int histerese = 20;

const unsigned long intervaloSerialMs = 200;
// -----------------------------------------------------

// Os dois motores usam slow decay no sentido "frente" (mais torque em PWM baixo).
// Se o motor 2 girar ao contrário, troque os dois fios dele na ponte H
// (não dá para inverter por software mantendo o mesmo modo de PWM).
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

// motorDireito/motorEsquerdo em função do lado físico do carrinho
void rodas(int esquerda, int direita) {
  motor1(direita);
  motor2(esquerda);
}

void frente(int v)        { rodas(v, v); }
void pivoDireita(int v)   { rodas(v, -v); }   // esquerda p/ frente, direita p/ trás
void pivoEsquerda(int v)  { rodas(-v, v); }
void parar()              { rodas(0, 0); }

// Leitura com histerese: dispara acima do limite, solta abaixo de limite-histerese
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

  if (direitaPreto && !esquerdaPreto) {
    // Linha escapou para a direita: pivô para a direita até o sensor limpar
    pivoDireita(velocidadeCurva);
    acao = "CURVA DIREITA";
  } else if (esquerdaPreto && !direitaPreto) {
    pivoEsquerda(velocidadeCurva);
    acao = "CURVA ESQUERDA";
  } else if (esquerdaPreto && direitaPreto) {
    // Cruzamento ou faixa larga: segue reto devagar
    frente(velocidadeReta);
    acao = "CRUZAMENTO";
  } else {
    frente(velocidadeReta);
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
  // Sem delay(): loop roda o mais rápido possível, reação imediata do sensor
}
