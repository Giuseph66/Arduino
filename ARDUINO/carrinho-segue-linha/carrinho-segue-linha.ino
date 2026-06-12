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
const int velocidadeBusca = 170;  // girando procurando a linha perdida

// Sensor 1 fica do lado DIREITO do carrinho? (true = sim)
// Sensor da direita vê preto -> linha fugiu p/ direita -> virar p/ direita.
const bool SENSOR1_E_O_DA_DIREITA = false;

// Se uma roda girar ao contrário no "frente", inverta o motor correspondente aqui.
const bool MOTOR1_INVERTIDO = false;
const bool MOTOR2_INVERTIDO = true;

// Leitura do sensor
const bool usarLeituraDigital = false; // DO deste módulo nunca dispara (trimpot); usar AO
const bool pretoQuandoDOBaixo = false; // false: DO HIGH = preto (padrão LM393)
const int  limiteAnalogico    = 80;    // branco lê ~11-58, preto sobe acima de 100
const bool pretoQuandoAOMaior = true;  // analógico maior = preto

// Correção leve: depois que o sensor limpa, completa a curva por este tempo.
const unsigned long tempoManterCurvaMs = 50;

// Curva fechada: se o sensor ficou no preto por mais que isto, não é correção
// leve — é quina/curva forte. Aí o carrinho continua girando ATÉ reencontrar
// a linha (sem limite de tempo), em vez de desistir e seguir reto.
const unsigned long limiarCurvaFechadaMs = 50;

const unsigned long intervaloSerialMs = 200;
// -----------------------------------------------------

bool ultimaCurvaDireita = true;
unsigned long manterCurvaAte = 0;
unsigned long inicioPreto = 0;   // quando o sensor entrou no preto
bool emCurva = false;            // sensor de um lado está no preto agora
bool buscandoLinha = false;      // curva fechada: girar até reencontrar a linha

// Os dois motores usam slow decay no sentido "frente" (mais torque em PWM baixo).
// Se o motor 2 girar ao contrário, troque os dois fios dele na ponte H
// (não dá para inverter por software mantendo o mesmo modo de PWM).
void motor1(int velocidade) {
  if (MOTOR1_INVERTIDO) velocidade = -velocidade;
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

bool lerPreto(int pinoDO, int pinoAO) {
  if (usarLeituraDigital) {
    int d = digitalRead(pinoDO);
    return pretoQuandoDOBaixo ? (d == LOW) : (d == HIGH);
  }
  int a = analogRead(pinoAO);
  return pretoQuandoAOMaior ? (a > limiteAnalogico) : (a < limiteAnalogico);
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
  bool s1Preto = lerPreto(sensor1DO, sensor1AO);
  bool s2Preto = lerPreto(sensor2DO, sensor2AO);

  bool direitaPreto  = SENSOR1_E_O_DA_DIREITA ? s1Preto : s2Preto;
  bool esquerdaPreto = SENSOR1_E_O_DA_DIREITA ? s2Preto : s1Preto;

  unsigned long agora = millis();
  const char *acao;

  if (direitaPreto && !esquerdaPreto) {
    // Linha escapou para a direita: pivô para a direita até o sensor limpar
    pivoDireita(velocidadeCurva);
    ultimaCurvaDireita = true;
    buscandoLinha = false;
    if (!emCurva) { emCurva = true; inicioPreto = agora; }
    acao = "CURVA DIREITA";
  } else if (esquerdaPreto && !direitaPreto) {
    pivoEsquerda(velocidadeCurva);
    ultimaCurvaDireita = false;
    buscandoLinha = false;
    if (!emCurva) { emCurva = true; inicioPreto = agora; }
    acao = "CURVA ESQUERDA";
  } else if (esquerdaPreto && direitaPreto) {
    // Cruzamento ou faixa larga: segue reto devagar
    frente(velocidadeReta);
    emCurva = false;
    buscandoLinha = false;
    acao = "CRUZAMENTO";
  } else {
    // Os dois no branco
    if (emCurva) {
      // Sensor acabou de limpar: decide pelo tempo que ficou no preto
      emCurva = false;
      if (agora - inicioPreto >= limiarCurvaFechadaMs) {
        buscandoLinha = true;            // curva fechada: gira até achar a linha
      } else {
        manterCurvaAte = agora + tempoManterCurvaMs; // correção leve: ponte curta
      }
    }

    if (buscandoLinha) {
      if (ultimaCurvaDireita) pivoDireita(velocidadeBusca);
      else                    pivoEsquerda(velocidadeBusca);
      acao = "BUSCANDO LINHA";
    } else if (agora < manterCurvaAte) {
      if (ultimaCurvaDireita) pivoDireita(velocidadeCurva);
      else                    pivoEsquerda(velocidadeCurva);
      acao = "COMPLETANDO CURVA";
    } else {
      frente(velocidadeReta);
      acao = "FRENTE";
    }
  }

  static unsigned long ultimaImpressao = 0;
  if (agora - ultimaImpressao >= intervaloSerialMs) {
    ultimaImpressao = agora;
    Serial.print("S1(DO=");
    Serial.print(digitalRead(sensor1DO));
    Serial.print(",AO=");
    Serial.print(analogRead(sensor1AO));
    Serial.print(")=");
    Serial.print(s1Preto ? "PRETO " : "BRANCO");
    Serial.print(" | S2(DO=");
    Serial.print(digitalRead(sensor2DO));
    Serial.print(",AO=");
    Serial.print(analogRead(sensor2AO));
    Serial.print(")=");
    Serial.print(s2Preto ? "PRETO " : "BRANCO");
    Serial.print(" | ");
    Serial.println(acao);
  }
  // Sem delay(): loop roda o mais rápido possível, reação imediata do sensor
}
