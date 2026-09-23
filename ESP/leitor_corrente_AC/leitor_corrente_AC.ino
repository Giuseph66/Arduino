#include <Arduino.h>
#include <math.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  const int PIN_SENSOR = 4;
#else
  const int PIN_SENSOR = 34;
#endif

#if defined(LED_BUILTIN)
  const int PIN_LED = LED_BUILTIN;
#else
  const int PIN_LED = 2;
#endif

const float LIMIAR_LED_VOLTS = 127.0;
const float LIMIAR_ZERO_VOLTS = 10.0;

// Dois resistores de 10k: divisor 1:2                                           
const float FATOR_DIVISOR = 2.0;

// Inicialmente deixe em 1.0.
// Substitua após a calibração.
//float FATOR_CALIBRACAO = 1.0;
float FATOR_CALIBRACAO = 594.7;

const unsigned long TEMPO_AMOSTRAGEM = 400;

float medirTensaoRMS() {
  double soma = 0;
  double somaQuadrados = 0;
  unsigned long amostras = 0;

  unsigned long inicio = millis();

  while (millis() - inicio < TEMPO_AMOSTRAGEM) {

    // Leitura calibrada do ADC em volts
    float tensaoADC =
        analogReadMilliVolts(PIN_SENSOR) / 1000.0;

    soma += tensaoADC;
    somaQuadrados +=
        (double)tensaoADC * tensaoADC;

    amostras++;

    delayMicroseconds(500);
  }

  if (amostras == 0) {
    return 0;
  }

  // Elimina o deslocamento DC da saída.
  double media = soma / amostras;

  double variancia =
      somaQuadrados / amostras -
      media * media;

  if (variancia < 0) {
    variancia = 0;
  }

  double rmsADC = sqrt(variancia);

  // Recupera a amplitude antes do divisor.
  return rmsADC * FATOR_DIVISOR;
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  analogReadResolution(12);

  analogSetPinAttenuation(
      PIN_SENSOR, ADC_11db
  );

  Serial.println("Sensor ZMPT101B");
  Serial.println("Inicializando...");

  delay(1000);
}

void loop() {
  float tensaoSensor = medirTensaoRMS();

  float tensaoAC =
      tensaoSensor * FATOR_CALIBRACAO;

  if (tensaoAC < LIMIAR_ZERO_VOLTS) {
    tensaoAC = 0.0;
  }

  digitalWrite(
      PIN_LED,
      tensaoAC >= LIMIAR_LED_VOLTS ? HIGH : LOW
  );

  Serial.print("Sinal RMS do modulo: ");
  Serial.print(tensaoSensor, 4);
  Serial.println(" V");

  if (FATOR_CALIBRACAO == 1.0) {
    Serial.println(
      "Tensao AC: aguardando calibracao"
    );
  } else {
    Serial.print("Tensao AC estimada: ");
    Serial.print(tensaoAC, 2);
    Serial.println(" V");
  }

  Serial.println("------------------");

  delay(100);
}
