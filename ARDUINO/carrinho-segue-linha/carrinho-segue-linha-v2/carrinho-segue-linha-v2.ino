// Carrinho segue-linha V2 — calibração automática no boot
// Motor 1: AIA=2, AIB=3 | Motor 2: BIA=4, BIB=5 | Sensores: AO em A0/A1
//
// USO: ligue com os sensores sobre a pista. LED piscando (5s) = deslize o
// carrinho passando os sensores sobre preto e branco. LED fixo 1s = pronto.
// Serial 115200 mostra limiares e leituras.

#define AIA 2
#define AIB 3
#define BIA 4
#define BIB 5

const int sensor1AO = A0;
const int sensor2AO = A1;

// ---------------- Ajustes principais ----------------
const int velocidadeReta  = 60;
const int velocidadeCurva = 150;

const bool SENSOR1_E_O_DA_DIREITA = false;

const bool MOTOR1_INVERTIDO = false;
const bool MOTOR2_INVERTIDO = true;

// Compensação de força por motor, em % (100 = sem ajuste)
const int ganhoMotor1Pct = 100;
const int ganhoMotor2Pct = 115;

const unsigned long tempoCalibracaoMs = 5000;
const unsigned long intervaloSerialMs = 200;
// -----------------------------------------------------

int limiarS1, limiarS2;
int histS1, histS2;

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

void calibrar() {
  int min1 = 1023, max1 = 0;
  int min2 = 1023, max2 = 0;

  unsigned long fim = millis() + tempoCalibracaoMs;
  while (millis() < fim) {
    int a1 = analogRead(sensor1AO);
    int a2 = analogRead(sensor2AO);
    if (a1 < min1) min1 = a1;
    if (a1 > max1) max1 = a1;
    if (a2 < min2) min2 = a2;
    if (a2 > max2) max2 = a2;
    digitalWrite(LED_BUILTIN, (millis() / 150) % 2);
  }

  // Limiar a 1/4 da faixa: o AO sobe devagar, em movimento o pico fica
  // bem abaixo do preto medido parado. Faixa pequena = usa reserva.
  if (max1 - min1 < 50) { limiarS1 = 70; histS1 = 10; }
  else { limiarS1 = min1 + (max1 - min1) / 4; histS1 = (max1 - min1) / 10; }

  if (max2 - min2 < 50) { limiarS2 = 50; histS2 = 10; }
  else { limiarS2 = min2 + (max2 - min2) / 4; histS2 = (max2 - min2) / 10; }

  Serial.print("Calibracao S1: min="); Serial.print(min1);
  Serial.print(" max="); Serial.print(max1);
  Serial.print(" limiar="); Serial.print(limiarS1);
  Serial.print(" hist="); Serial.println(histS1);
  Serial.print("Calibracao S2: min="); Serial.print(min2);
  Serial.print(" max="); Serial.print(max2);
  Serial.print(" limiar="); Serial.print(limiarS2);
  Serial.print(" hist="); Serial.println(histS2);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
}

bool lerPreto(int pinoAO, int limiar, int hist, bool &estavaPreto) {
  int a = analogRead(pinoAO);
  if (estavaPreto) {
    if (a < limiar - hist) estavaPreto = false;
  } else {
    if (a > limiar + hist) estavaPreto = true;
  }
  return estavaPreto;
}

void setup() {
  Serial.begin(115200);

  pinMode(AIA, OUTPUT);
  pinMode(AIB, OUTPUT);
  pinMode(BIA, OUTPUT);
  pinMode(BIB, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  parar();
  calibrar();
}

void loop() {
  static bool s1Estado = false;
  static bool s2Estado = false;
  bool s1Preto = lerPreto(sensor1AO, limiarS1, histS1, s1Estado);
  bool s2Preto = lerPreto(sensor2AO, limiarS2, histS2, s2Estado);

  bool direitaPreto  = SENSOR1_E_O_DA_DIREITA ? s1Preto : s2Preto;
  bool esquerdaPreto = SENSOR1_E_O_DA_DIREITA ? s2Preto : s1Preto;

  digitalWrite(LED_BUILTIN, (s1Preto || s2Preto) ? HIGH : LOW);

  // Dois pretos = repete a última ação (curva fechada entra em diagonal
  // sob os dois sensores; cruzamento real segue reto)
  static int ultimaAcao = 0; // 0 = frente, 1 = direita, 2 = esquerda

  const char *acao;
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
  unsigned long agora = millis();
  if (agora - ultimaImpressao >= intervaloSerialMs) {
    ultimaImpressao = agora;
    Serial.print("S1=");
    Serial.print(analogRead(sensor1AO));
    Serial.print(s1Preto ? "(P)" : "(B)");
    Serial.print(" S2=");
    Serial.print(analogRead(sensor2AO));
    Serial.print(s2Preto ? "(P)" : "(B)");
    Serial.print(" | ");
    Serial.println(acao);
  }
}
