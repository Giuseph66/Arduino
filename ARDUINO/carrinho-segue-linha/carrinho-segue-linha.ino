#define AIA 2
#define AIB 3
#define BIA 4
#define BIB 5

const int sensor1DO = 6;
const int sensor2DO = 7;

const int sensor1AO = A0;
const int sensor2AO = A1;

const int intervaloLeituraMs = 1;
const unsigned long intervaloSerialMs = 100;
const int velocidadeReta = 100;
const int velocidadeCurva = 80;

const int limiteSensor1 = 100;
const int limiteSensor2 = 100;
const bool pretoQuandoAOMaior = true;
const bool usarLeituraDigital = true;
const bool pretoQuandoDOBaixo = false;
const unsigned long tempoManterCurvaMs = 250;
bool ultimaCurvaDireita = true;
unsigned long manterCurvaAte = 0;

void motor1(int velocidade) {
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
  velocidade = constrain(velocidade, -255, 255);

  if (velocidade > 0) {
    digitalWrite(BIA, LOW);
    analogWrite(BIB, velocidade);
  } else if (velocidade < 0) {
    digitalWrite(BIA, HIGH);
    analogWrite(BIB, 255 + velocidade);
  } else {
    digitalWrite(BIA, LOW);
    analogWrite(BIB, 0);
  }
}

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
  motor1(velocidadeReta);
  motor2(velocidadeReta);
}

void parar() {
  motor1(0);
  motor2(0);
}

void virarEsquerda() {
  motor1(-velocidadeCurva);
  motor2(velocidadeCurva);
}

void virarDireita() {
  motor1(velocidadeCurva);
  motor2(-velocidadeCurva);
}

void loop() {
  int s1Digital = digitalRead(sensor1DO);
  int s2Digital = digitalRead(sensor2DO);

  int s1Analogico = analogRead(sensor1AO);
  int s2Analogico = analogRead(sensor2AO);

  bool s1Preto = pretoQuandoAOMaior ? s1Analogico > limiteSensor1 : s1Analogico < limiteSensor1;
  bool s2Preto = pretoQuandoAOMaior ? s2Analogico > limiteSensor2 : s2Analogico < limiteSensor2;
  if (usarLeituraDigital) {
    s1Preto = pretoQuandoDOBaixo ? s1Digital == LOW : s1Digital == HIGH;
    s2Preto = pretoQuandoDOBaixo ? s2Digital == LOW : s2Digital == HIGH;
  }

  bool motor1Ligado = false;
  bool motor2Ligado = false;
  const char *acao = "PARAR";
  unsigned long agora = millis();

  if (s1Preto && !s2Preto) {
    virarDireita();
    ultimaCurvaDireita = true;
    manterCurvaAte = agora + tempoManterCurvaMs;
    motor1Ligado = true;
    motor2Ligado = true;
    acao = "VIRAR DIREITA";
  } else if (!s1Preto && s2Preto) {
    virarEsquerda();
    ultimaCurvaDireita = false;
    manterCurvaAte = agora + tempoManterCurvaMs;
    motor1Ligado = true;
    motor2Ligado = true;
    acao = "VIRAR ESQUERDA";
  } else if (!s1Preto && !s2Preto) {
    if (agora < manterCurvaAte) {
      if (ultimaCurvaDireita) {
        virarDireita();
        acao = "MANTER DIREITA";
      } else {
        virarEsquerda();
        acao = "MANTER ESQUERDA";
      }
    } else {
      frente();
      acao = "FRENTE";
    }
    motor1Ligado = true;
    motor2Ligado = true;
  } else {
    if (ultimaCurvaDireita) {
      virarDireita();
      acao = "BUSCAR DIREITA";
    } else {
      virarEsquerda();
      acao = "BUSCAR ESQUERDA";
    }
    motor1Ligado = true;
    motor2Ligado = true;
  }

  static unsigned long ultimaImpressaoSerial = 0;
  if (agora - ultimaImpressaoSerial >= intervaloSerialMs) {
    ultimaImpressaoSerial = agora;

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
    Serial.print(motor2Ligado ? "GIRANDO" : "PARADO");
    Serial.print(" || ACAO: ");
    Serial.println(acao);
  }

  delay(intervaloLeituraMs);
}
