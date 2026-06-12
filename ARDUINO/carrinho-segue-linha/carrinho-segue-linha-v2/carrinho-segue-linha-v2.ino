// Carrinho segue-linha V2 — 2 sensores IR analógicos + ponte H L9110 + Nano
// Baseado no padrão clássico de 2 sensores (Circuit Digest / Arduino Project
// Hub / QTR Pololu): lógica reativa pura + CALIBRAÇÃO AUTOMÁTICA no boot.
//
// PORTAS FIXAS:
//   Motor 1: AIA=2 (direção), AIB=3 (PWM)
//   Motor 2: BIA=4 (direção), BIB=5 (PWM)
//   Sensores: AO em A0/A1 (DO não é usado)
//
// COMO USAR:
//   1. Ligue o carrinho COM OS SENSORES SOBRE A PISTA.
//   2. O LED do Nano (pino 13) começa a piscar: você tem 5 segundos para
//      deslizar o carrinho de um lado para o outro, passando os dois
//      sensores sobre o preto e o branco algumas vezes.
//   3. LED acende fixo por 1s = calibrado. Depois apaga e o carrinho anda.
//   4. Serial 115200 mostra os limiares calculados e as leituras ao vivo.
//
// Sem nenhum delay no loop: leitura -> decisão -> motor, direto.

#define AIA 2
#define AIB 3
#define BIA 4
#define BIB 5

const int sensor1AO = A0; // sensor do módulo no D6 (lado esquerdo)
const int sensor2AO = A1; // sensor do módulo no D7 (lado direito)

// ---------------- Ajustes principais ----------------
const int velocidadeReta  = 60;
const int velocidadeCurva = 150;

// Sensor 1 fica do lado DIREITO do carrinho? (true = sim)
const bool SENSOR1_E_O_DA_DIREITA = false;

// Sentido dos motores (mesma configuração que funcionou na V1)
const bool MOTOR1_INVERTIDO = false;
const bool MOTOR2_INVERTIDO = true;

// Compensação de força por motor, em % (100 = sem ajuste)
const int ganhoMotor1Pct = 100;
const int ganhoMotor2Pct = 115;

// Calibração automática
const unsigned long tempoCalibracaoMs = 5000;

const unsigned long intervaloSerialMs = 200;
// -----------------------------------------------------

// Limiares calculados na calibração (meio entre branco e preto de cada sensor)
int limiarS1, limiarS2;
int histS1, histS2; // histerese = 1/8 da faixa medida de cada sensor

// ---------------- Motores ----------------
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

// ---------------- Calibração ----------------
// Mede o mínimo (branco) e o máximo (preto) de cada sensor enquanto o
// usuário desliza o carrinho sobre a linha. Limiar = ponto médio real.
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
    digitalWrite(LED_BUILTIN, (millis() / 150) % 2); // pisca = calibrando
  }

  // Limiar a 1/4 da faixa (perto do branco), não no meio: o AO destes módulos
  // sobe devagar e, com o carrinho em movimento, o pico real fica bem abaixo
  // do preto medido parado. Limiar baixo = dispara mais cedo.
  // Faixa pequena = sensor não passou pelo preto. Usa valores de reserva.
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

  digitalWrite(LED_BUILTIN, HIGH); // fixo = pronto
  delay(1000);                     // tempo de posicionar o carrinho
  digitalWrite(LED_BUILTIN, LOW);
}

// Histerese simétrica: vira preto acima de (limiar+hist),
// volta a branco abaixo de (limiar-hist). Não trava nem treme.
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

  // LED aceso = algum sensor vendo preto (debug visual sem PC)
  digitalWrite(LED_BUILTIN, (s1Preto || s2Preto) ? HIGH : LOW);

  // Última ação: 0 = frente, 1 = direita, 2 = esquerda.
  // Dois sensores no preto = repete a última ação: numa curva fechada a
  // linha entra em diagonal sob os dois sensores (não é cruzamento — tem
  // que continuar virando). Num cruzamento real estava indo reto e segue reto.
  static int ultimaAcao = 0;

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
