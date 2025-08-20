#include <DS3231.h>  
#include <HCSR04.h>

// Relógio DS3231
DS3231 rtc(SDA, SCL);
Time t;

// Sensor ultrassônico
const byte TRIG_PIN = 9;
const byte ECHO_PIN = 10;
UltraSonicDistanceSensor sensor(TRIG_PIN, ECHO_PIN);

// LEDs
const int ledVerde = 2;
const int ledAmarelo = 3;
const int ledVermelho1 = 4;
const int ledVermelho2 = 5;

// Relé de alimentação
const int relePin = 8;

// Horários de alimentação
int horaAlimentacao1 = 20;
int minutoAlimentacao1 = 35;
int demosComida1 = 0;

int horaAlimentacao2 = 30;
int minutoAlimentacao2 = 30;
int demosComida2 = 0;

// Níveis de distância
const float distanciaAlto = 10.0;
const float distanciaMedio = 25.0;
const float distanciaBaixo = 40.0;

// Piscar LED
bool piscarEstado = false;
unsigned long ultimoPiscar = 0;
const unsigned long intervaloPiscar = 1000;

void desligarLEDs() {
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledAmarelo, LOW);
  digitalWrite(ledVermelho1, LOW);
  digitalWrite(ledVermelho2, LOW);
}

void setup() {
  rtc.begin();
  Serial.begin(115200);

  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, HIGH); // desliga o relé

  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVermelho1, OUTPUT);
  pinMode(ledVermelho2, OUTPUT);

  // Apenas para configurar o relógio uma vez:
  rtc.setDate(24, 6, 2025);    // dia, mês, ano
  rtc.setDOW(TUESDAY);         // dia da semana
  rtc.setTime(20, 30, 0);      // hora, minuto, segundo
}

void loop() {
  // Obter horário atual
  t = rtc.getTime();
  int horaAtual = t.hour;
  int minutoAtual = t.min;

  // Alimentação 1
  if (horaAtual == horaAlimentacao1 && minutoAtual == minutoAlimentacao1 && demosComida1 == 0) {
    digitalWrite(relePin, LOW);
    delay(20000);
    digitalWrite(relePin, HIGH);
    demosComida1 = 1;
  }

  // Alimentação 2
  if (horaAtual == horaAlimentacao2 && minutoAtual == minutoAlimentacao2 && demosComida2 == 0) {
    digitalWrite(relePin, LOW);
    delay(20000);
    digitalWrite(relePin, HIGH);
    demosComida2 = 1;
  }

  // Resetar meia-noite
  if (horaAtual == 0 && minutoAtual == 0) {
    demosComida1 = 0;
    demosComida2 = 0;
  }

  // Mostrar próxima alimentação
  Serial.print("Horário atual: ");
  Serial.println(rtc.getTimeStr());

  if (demosComida1 == 0) {
    Serial.print("Próxima alimentação: ");
    Serial.print(horaAlimentacao1); Serial.print("h:");
    Serial.println(minutoAlimentacao1);
  } else if (demosComida2 == 0) {
    Serial.print("Próxima alimentação: ");
    Serial.print(horaAlimentacao2); Serial.print("h:");
    Serial.println(minutoAlimentacao2);
  } else {
    Serial.println("Alimentações do dia completas.");
  }

  Serial.println("---------------");

  // Leitura do nível com sensor ultrassônico
  float distancia = sensor.measureDistanceCm();

  Serial.print("Nível (cm): ");
  Serial.println(distancia);

  desligarLEDs();

  if (distancia <= distanciaAlto) {
    digitalWrite(ledVerde, HIGH);
  } else if (distancia <= distanciaMedio) {
    digitalWrite(ledAmarelo, HIGH);
  } else if (distancia <= distanciaBaixo) {
    digitalWrite(ledVermelho1, HIGH);
  } else {
    unsigned long agora = millis();
    if (agora - ultimoPiscar >= intervaloPiscar) {
      piscarEstado = !piscarEstado;
      digitalWrite(ledVermelho1, piscarEstado ? HIGH : LOW);
      digitalWrite(ledVermelho2, piscarEstado ? LOW : HIGH);
      ultimoPiscar = agora;
    }
  }

  delay(1000); // tempo entre ciclos
}
