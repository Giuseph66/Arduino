#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"   // FreqS=25 e BUFFER_SIZE=100
#include <math.h>

#define I2C_SDA 21
#define I2C_SCL 22

// Parâmetros do sensor (quando há dedo)
static const byte LED_BRIGHT_PRESENT = 0x18; // ajuste fino: 0x10..0x2F
static const byte SAMPLE_AVG         = 8;    // 8..16 = mais estável
static const byte LED_MODE           = 2;    // RED + IR
static const byte SAMPLE_RATE        = 25;   // TEM QUE SER 25 para a lib
static const int  PULSE_WIDTH        = 411;  // mais energia por pulso
static const int  ADC_RANGE          = 8192; // 4096/8192 ajuda a não saturar

// Parâmetros quando NÃO há dedo (economia)
static const byte LED_BRIGHT_ABSENT  = 0x02;

// Janela deslizante (~1 s por atualização)
#define STEP (BUFFER_SIZE/4)  // 25 amostras

// Heurísticas de presença/qualidade (ajuste conforme seu módulo/ambiente)
#define LED_FIXED         0x18   // LED fixo enquanto medindo
#define IR_MEAN_MIN       15000UL
#define IR_MEAN_MAX       80000UL
#define RED_MEAN_MIN       5000UL
#define RED_MEAN_MAX      80000UL
#define IR_PP_MIN          1500UL
#define IR_PP_MAX         12000UL
#define RED_PP_MIN          600UL
#define RED_PP_MAX        10000UL
#define ABSENCE_RESET_WINDOWS 3 // após ~3 s sem dedo, zera EMA/buffers

// Qualidade de sinal (usada em logs e checagens auxiliares)
#define IR_ACDC_MIN 0.06f      // 6%
#define IR_ACDC_MAX 0.60f      // 60%
#define IR_MEAN_TARGET_MIN 25000UL
#define IR_MEAN_TARGET_MAX 60000UL
// Limite de salto de HR vs EMA
#define HR_JUMP_LIM 25

// Intervalo entre leituras/report (ms)
#define READ_INTERVAL_MS 5000UL

// Expiração de valores suavizados (se nenhum valor válido chegar nesse tempo)
#define STALE_MS 15000UL

// Gating de HR
#define HR_MIN 40
#define HR_MAX 180
#define HR_JUMP_MAX 25         // rejeita saltos > 25 bpm por janela

MAX30105 sensor;
uint32_t irBuf[BUFFER_SIZE];
uint32_t redBuf[BUFFER_SIZE];

float bpmEMA  = -1;
float spo2EMA = -1;
int   absenceCount = 0;

// LED atual para modo "presente" (auto-ajustável)
static byte ledPresentCurrent = LED_BRIGHT_PRESENT;

struct Stats { uint32_t mean, pp; };

static inline Stats statsOf(const uint32_t *v, int n) {
  uint32_t mn = UINT32_MAX, mx = 0, sum = 0;
  for (int i = 0; i < n; i++) {
    uint32_t x = v[i];
    if (x < mn) mn = x;
    if (x > mx) mx = x;
    sum += x;
  }
  Stats s; s.mean = sum / (float)n; s.pp = mx - mn;
  return s;
}

static inline bool fingerPresent(const uint32_t *ir, int n, uint32_t &meanOut, uint32_t &ppOut) {
  Stats s = statsOf(ir, n);
  meanOut = s.mean;
  ppOut   = s.pp;
  return (meanOut > IR_MEAN_MIN) && (ppOut > IR_PP_MIN);
}

static inline bool qualityOK(const Stats& ir, const Stats& rd) {
  float rIR = (ir.mean > 0) ? (float)ir.pp / (float)ir.mean : 0.f;
  float rRD = (rd.mean > 0) ? (float)rd.pp / (float)rd.mean : 0.f;

  bool irOK  = ir.mean >= IR_MEAN_MIN  && ir.mean <= IR_MEAN_MAX  &&
               ir.pp   >= IR_PP_MIN    && ir.pp   <= IR_PP_MAX    &&
               rIR     >= IR_ACDC_MIN  && rIR     <= IR_ACDC_MAX;

  bool rdOK  = rd.mean >= RED_MEAN_MIN && rd.mean <= RED_MEAN_MAX &&
               rd.pp   >= RED_PP_MIN   && rd.pp   <= RED_PP_MAX   &&
               rRD     >= (IR_ACDC_MIN/3.0f) && rRD     <= IR_ACDC_MAX;

  return irOK && rdOK;
}

// Estima HR simples a partir do IR usando picos com período mínimo (refratário)
static int estimateHrFromIR(const uint32_t *ir, int n, float sampleRate, uint32_t mean, float rms) {
  if (n < 5) return -1;
  const float threshold = rms * 0.5f; // metade do RMS acima do mean
  const int minDist = (int)(sampleRate * 60.0f / (float)HR_MAX); // ~8 amostras em 25Hz para 180 bpm
  if (minDist < 2) {
    // segurança mínima
    // evita marcar picos adjacentes muito próximos
  }
  int lastPeak = -100000;
  int peakIdxs[32];
  int peakCount = 0;
  for (int i = 1; i < n - 1 && peakCount < 32; i++) {
    float v = (float)ir[i] - (float)mean;
    if (v > threshold && ir[i] > ir[i - 1] && ir[i] >= ir[i + 1]) {
      if (i - lastPeak >= minDist) {
        peakIdxs[peakCount++] = i;
        lastPeak = i;
      }
    }
  }
  if (peakCount < 2) return -1;
  int intervals[31];
  int m = 0;
  for (int i = 1; i < peakCount; i++) {
    intervals[m++] = peakIdxs[i] - peakIdxs[i - 1];
  }
  // mediana dos intervalos
  for (int i = 0; i < m - 1; i++) {
    for (int j = i + 1; j < m; j++) {
      if (intervals[j] < intervals[i]) { int t = intervals[i]; intervals[i] = intervals[j]; intervals[j] = t; }
    }
  }
  int med = intervals[m / 2];
  if (med <= 0) return -1;
  float hr = 60.0f * sampleRate / (float)med;
  int hrInt = (int)(hr + 0.5f);
  if (hrInt < HR_MIN || hrInt > HR_MAX) return -1;
  return hrInt;
}

void setLedBrightness(byte v) {
  sensor.setPulseAmplitudeRed(v);
  sensor.setPulseAmplitudeIR(v);
  sensor.setPulseAmplitudeGreen(0);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("Inicializando MAX3010x...");
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 nao encontrado.");
    while (1) delay(10);
  }

  sensor.setup(LED_FIXED, SAMPLE_AVG, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
  sensor.setPulseAmplitudeRed(LED_FIXED);
  sensor.setPulseAmplitudeIR(LED_FIXED);
  sensor.setPulseAmplitudeGreen(0);
  ledPresentCurrent = LED_FIXED;

  Serial.println("Sensor iniciado. Aproxime o dedo (cubra da luz). Aguardando estabilizar...");

  // Preenche buffer inicial (4 s)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (!sensor.available()) sensor.check();
    redBuf[i] = sensor.getRed();
    irBuf[i]  = sensor.getIR();
    sensor.nextSample();

    if (((i + 1) % 10) == 0 || (i + 1) == BUFFER_SIZE) {
      Serial.printf("Preenchendo buffer inicial: %d/%d\n", i + 1, BUFFER_SIZE);
    }
  }

  Serial.println("Buffer inicial preenchido. Iniciando janelas de leitura...");
}

void loop() {
  // LED fixo durante a medição desta janela
  setLedBrightness(LED_FIXED);

  // Contador e temporização de report
  static uint32_t windowIndex = 0;
  static uint32_t lastReportMs = 0;

  // Lê STEP novas amostras
  uint32_t redNew[STEP], irNew[STEP];
  for (int i = 0; i < STEP; i++) {
    while (!sensor.available()) sensor.check();
    redNew[i] = sensor.getRed();
    irNew[i]  = sensor.getIR();
    sensor.nextSample();
  }

  // Janela deslizante
  memmove(redBuf, redBuf + STEP, (BUFFER_SIZE - STEP) * sizeof(uint32_t));
  memmove(irBuf,  irBuf  + STEP, (BUFFER_SIZE - STEP) * sizeof(uint32_t));
  for (int i = 0; i < STEP; i++) {
    redBuf[BUFFER_SIZE - STEP + i] = redNew[i];
    irBuf [BUFFER_SIZE - STEP + i] = irNew[i];
  }

  // Detecta presença do dedo e estatísticas da janela atual
  uint32_t irMean, irPP;
  bool present = fingerPresent(irBuf, BUFFER_SIZE, irMean, irPP);
  Stats irS = { irMean, irPP };
  Stats rdS = statsOf(redBuf, BUFFER_SIZE);
  // AC como RMS ao invés de pico-a-pico (mais robusto a outliers)
  double sumSq = 0.0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    double dv = (double)irBuf[i] - (double)irMean;
    sumSq += dv * dv;
  }
  double rms = sqrt(sumSq / (double)BUFFER_SIZE);
  float acdc = (irMean > 0) ? ((float)rms / (float)irMean) : 0.0f;

  if (!present) {
    // Sem dedo: LEDs fracos, zera depois de alguns ciclos e não atualiza EMA
    absenceCount++;
    setLedBrightness(LED_BRIGHT_ABSENT);

    if (absenceCount >= ABSENCE_RESET_WINDOWS) {
      bpmEMA = spo2EMA = -1;
      sensor.clearFIFO(); // limpa lixo acumulado
    }

    // STATUS e linha final apenas a cada READ_INTERVAL_MS
    if (millis() - lastReportMs >= READ_INTERVAL_MS) {
      lastReportMs = millis();
      Serial.printf("STATUS j=%lu present=0 abs=%d irMean=%lu irPP=%lu led=0x%02X\n",
                    (unsigned long)windowIndex, absenceCount,
                    (unsigned long)irMean, (unsigned long)irPP, LED_BRIGHT_ABSENT);
      Serial.println("BPM: -1 | SpO2: -1 %");
    }
    windowIndex++;
    return; // pula cálculo
  }

  // Com dedo: garante LEDs “normais” e zera contador de ausência
  if (absenceCount > 0) {
    absenceCount = 0;
    setLedBrightness(LED_FIXED);
  }

  // Auto-ajuste desabilitado (LED fixo). Manter futuro gancho se quisermos reativar.

  // Checagem de qualidade de janela (antes de calcular HR/SpO2)
  bool okQual = qualityOK(irS, rdS);
  if (!okQual) {
    if (millis() - lastReportMs >= READ_INTERVAL_MS) {
      lastReportMs = millis();
      Serial.printf("STATUS j=%lu present=%d abs=%d meanIR=%lu ppIR=%lu meanR=%lu ppR=%lu acdcIR=%.3f led=0x%02X QUAL=BAD\n",
                    (unsigned long)windowIndex, (int)present, absenceCount,
                    (unsigned long)irS.mean, (unsigned long)irS.pp,
                    (unsigned long)rdS.mean, (unsigned long)rdS.pp,
                    (irS.mean>0? (float)irS.pp/(float)irS.mean:0.0f), LED_FIXED);
      Serial.println("BPM: -1 | SpO2: -1 %");
    }
    windowIndex++;
    return;
  }

  // Calcula HR/SpO2
  int32_t spo2, hr;
  int8_t validSpo2, validHr;
  maxim_heart_rate_and_oxygen_saturation(irBuf, BUFFER_SIZE, redBuf,
                                         &spo2, &validSpo2, &hr, &validHr);
  int hrAux = estimateHrFromIR(irBuf, BUFFER_SIZE, SAMPLE_RATE, irMean, (float)rms);

  // Validação extra (gating) e heurísticas para HR
  static int lastAcceptedHr = -1;
  bool acdcOk = (acdc >= IR_ACDC_MIN) && (acdc <= IR_ACDC_MAX);
  int hrUsed = (int)hr;
  bool hrOk   = validHr && (hrUsed >= HR_MIN && hrUsed <= HR_MAX) && acdcOk;
  bool spo2Ok = validSpo2 && (spo2 >= 80 && spo2 <= 100) && acdcOk;

  // fallback para estimativa auxiliar por picos
  if ((!hrOk || !validHr) && hrAux > 0) {
    hrUsed = hrAux;
    hrOk = (hrUsed >= HR_MIN && hrUsed <= HR_MAX) && acdcOk;
  }

  // Heurística de dobra (quando for ~2x do histórico/EMA)
  if (hrUsed > 120) {
    bool halve = false;
    if (bpmEMA > 0) {
      int twiceEma = 2 * (int)bpmEMA;
      if (fabs((double)hrUsed - (double)twiceEma) <= 0.2 * (double)twiceEma) halve = true;
    }
    if (!halve && lastAcceptedHr > 0) {
      int twiceLast = 2 * lastAcceptedHr;
      if (fabs((double)hrUsed - (double)twiceLast) <= 0.2 * (double)twiceLast) halve = true;
    }
    if (halve) hrUsed = hrUsed / 2;
  }

  if (hrOk && lastAcceptedHr > 0 && abs(hrUsed - lastAcceptedHr) > HR_JUMP_MAX) {
    hrOk = false; // rejeita salto brusco
  }
  bool ok = hrOk && spo2Ok;

  static uint32_t lastValidMs = 0;
  if (ok) {
    // Suavização (EMA)
    bpmEMA  = (bpmEMA  < 0) ? hrUsed : 0.7f * bpmEMA  + 0.3f * hrUsed;
    spo2EMA = (spo2EMA < 0) ? spo2   : 0.7f * spo2EMA + 0.3f * spo2;
    lastAcceptedHr = hrUsed;
    lastValidMs = millis();
  }
  // Expira EMAs após STALE_MS sem novos dados válidos
  if (lastValidMs > 0 && (millis() - lastValidMs) > STALE_MS) {
    bpmEMA = -1;
    spo2EMA = -1;
  }

  // STATUS e exibição somente a cada READ_INTERVAL_MS
  if (millis() - lastReportMs >= READ_INTERVAL_MS) {
    lastReportMs = millis();
    Serial.printf("STATUS j=%lu present=1 abs=%d irMean=%lu irPP=%lu acdc=%.3f led=0x%02X HRlib=%ld(valid=%d) HRaux=%d HRused=%d SpO2=%ld(valid=%d) hrOk=%d spo2Ok=%d EMA_HR=%d EMA_SpO2=%d\n",
                  (unsigned long)windowIndex, absenceCount,
                  (unsigned long)irMean, (unsigned long)irPP, acdc, ledPresentCurrent,
                  (long)hr, (int)validHr, (int)hrAux, (int)hrUsed, (long)spo2, (int)validSpo2,
                  (int)hrOk, (int)spo2Ok,
                  (int)(bpmEMA > 0 ? (int)(bpmEMA + 0.5f) : -1),
                  (int)(spo2EMA > 0 ? (int)(spo2EMA + 0.5f) : -1));

    Serial.print("BPM: ");
    if (bpmEMA > 0) Serial.print((int)(bpmEMA + 0.5f));
    else             Serial.print(-1);

    Serial.print(" | SpO2: ");
    if (spo2EMA > 0) Serial.print((int)(spo2EMA + 0.5f));
    else              Serial.print(-1);
    Serial.println(" %");
  }
  windowIndex++;
}
