/*  2MAV-PLL para EMG – Arduino Nano (ATmega328P, 16 MHz)
    - fs = 2000 Hz (Timer1 CTC)
    - PLL com filtro MAV no detector de fase (PD)
    - Cancelamento adaptativo: y = x - MAV_N(x), N ≈ round(fs / f_hat)
    - NCO com LUT de seno (256 pontos), nada de sin() na ISR
    - Uso acadêmico/protótipo — não é dispositivo médico.

    Pino de leitura: A0
    Serial: 115200 baud (stream t(ms),x,y,f_hat)
*/

#include <Arduino.h>
#include <avr/pgmspace.h>

// ===================== Configuração =====================
#define EMG_PIN       A0
#define FS_HZ         2000UL            // taxa de amostragem
#define F_CPU_HZ      16000000UL
#define TIMER1_PRESC  8                 // 16 MHz/8 = 2 MHz (0,5 us)
#define NET_FREQ_INIT 60.0f             // 60 Hz no Brasil (troque pra 50.0f se necessário)
#define F_PLL_MIN     45.0f
#define F_PLL_MAX     70.0f

// MAV do detector de fase (corte ~120 Hz): M1 = fs/120 ≈ 17
#define MAV1_LEN      (FS_HZ/120U)      // arredonda pra inteiro
#if (MAV1_LEN < 3) || (MAV1_LEN > 32)
#undef MAV1_LEN
#define MAV1_LEN 17
#endif

// MAV adaptativo: N ≈ fs/f_hat (limites)
#define N_MAX         200               // ~ fs/10
#define N_MIN         4

// Ganhos do PLL (começo seguro p/ fs=2 kHz; ajuste fino depois)
static const float PLL_KP = 1.2f;
static const float PLL_KI = (60.0f / (float)FS_HZ); // integral lenta

// HP 1ª ordem p/ offset/deriva (<~5 Hz) — EMG é >20 Hz
static const float HP_A = 0.995f;

// ===================== NCO (LUT de seno) =====================
// 256 amostras em Q15 [-32767..32767]
int16_t sinLUT[256];

// Gera LUT (uma vez no setup)
void buildSinLUT() {
  for (uint16_t i = 0; i < 256; ++i) {
    float ang = (2.0f * PI * (float)i) / 256.0f;
    int32_t q15 = (int32_t)roundf(sinf(ang) * 32767.0f);
    if (q15 > 32767) q15 = 32767;
    if (q15 < -32767) q15 = -32767;
    sinLUT[i] = (int16_t)q15;
  }
}

// ===================== Estado (amostragem/PLL) =====================
volatile bool   sampleReady = false;
volatile int16_t x_n = 0;         // amostra centrada
volatile int16_t y_n = 0;         // saída filtrada
volatile float  f_hat = NET_FREQ_INIT;
volatile uint32_t phase = 0;      // acumulador de fase (32 bits)
volatile uint32_t ph_inc = 0;     // incremento por amostra (NCO)

// MAV1 (detector de fase)
int32_t mav1Sum = 0;
int16_t mav1Buf[MAV1_LEN];
uint8_t mav1Pos = 0;

// MAV adaptativo N
int32_t mavNSum = 0;
int16_t mavNBuf[N_MAX];
uint16_t mavNPos = 0;
uint16_t N_curr = 16;

// HP 1ª ordem
float hp_x1 = 0.0f, hp_y1 = 0.0f;

// Controlador integral do PLL
float pllInt = 0.0f;

// Constante para ph_inc = f_hat * K (evita divisão cara na ISR)
const float NCO_K = 4294967296.0f / (float)FS_HZ; // 2^32 / fs

// ===================== Utilitários =====================
inline int16_t q15_mul(int16_t a, int16_t b) {
  // (a*b)>>15 em 32 bits para evitar overflow
  int32_t v = (int32_t)a * (int32_t)b;
  return (int16_t)(v >> 15);
}

inline int16_t clamp16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void resetMAV1() {
  mav1Sum = 0; mav1Pos = 0;
  for (uint8_t i = 0; i < MAV1_LEN; ++i) mav1Buf[i] = 0;
}

void resetMAVN(uint16_t N) {
  if (N < N_MIN) N = N_MIN;
  if (N > N_MAX) N = N_MAX;
  N_curr = N; mavNPos = 0; mavNSum = 0;
  for (uint16_t i = 0; i < N_MAX; ++i) mavNBuf[i] = 0;
}

inline int16_t mav1_step(int16_t v) {
  mav1Sum -= mav1Buf[mav1Pos];
  mav1Buf[mav1Pos] = v;
  mav1Sum += v;
  mav1Pos++;
  if (mav1Pos >= MAV1_LEN) mav1Pos = 0;
  // média inteira (ok porque PD vai pra float depois)
  return (int16_t)(mav1Sum / (int32_t)MAV1_LEN);
}

inline int16_t mavN_step(int16_t v) {
  // janela N_curr (variável)
  mavNSum -= mavNBuf[mavNPos];
  mavNBuf[mavNPos] = v;
  mavNSum += v;
  mavNPos++;
  if (mavNPos >= N_curr) mavNPos = 0;
  return (int16_t)(mavNSum / (int32_t)N_curr);
}

inline void updateNbyF(float f) {
  uint16_t N = (uint16_t)roundf((float)FS_HZ / f);
  if (N < N_MIN) N = N_MIN;
  if (N > N_MAX) N = N_MAX;
  if (N != N_curr) resetMAVN(N);
}

// ===================== Timer1 ISR (2 kHz) =====================
ISR(TIMER1_COMPA_vect) {
  // 1) Leitura rápida do ADC (analogRead bloqueia ~100 us, mas cabe em 500 us de orçamento)
  int raw = analogRead(EMG_PIN);     // 0..1023
  int16_t xi = (int16_t)raw - 512;   // centra em 0

  // HP digital simples (float, barato o suficiente a 2 kHz)
  float xf = (float)xi;
  float yhp = HP_A * (hp_y1 + xf - hp_x1);
  hp_x1 = xf; hp_y1 = yhp;
  int16_t x = clamp16((int32_t)roundf(yhp));

  // 2) NCO (seno de referência)
  // phase top-8 bits -> índice 0..255
  uint8_t idx = (uint8_t)(phase >> 24);
  int16_t s = sinLUT[idx];

  // 3) Detector de fase: PD bruto = x * s (Q15)
  int16_t pd_raw = q15_mul(x, s);

  // 4) LF do PLL: MAV1 no PD
  int16_t pd_filt = mav1_step(pd_raw);

  // 5) Controlador do PLL (PI em float)
  // escala PD para [-1..1] a partir de Q15
  float pd = (float)pd_filt / 32768.0f;
  pllInt += PLL_KI * pd;
  float df = PLL_KP * pd + pllInt;

  // 6) Atualiza f_hat e NCO inc
  float fnew = clampf(NET_FREQ_INIT + df, F_PLL_MIN, F_PLL_MAX);
  f_hat = fnew;
  ph_inc = (uint32_t)(f_hat * NCO_K);

  // 7) Integra fase
  phase += ph_inc;

  // 8) Atualiza N do MAV adaptativo e cancela: y = x - MAV_N(x)
  updateNbyF(f_hat);
  int16_t est = mavN_step(x);
  int16_t y = clamp16((int32_t)x - (int32_t)est);

  // 9) Publica
  x_n = x;
  y_n = y;
  sampleReady = true;
}

// ===================== Setup/Loop =====================
void setupTimer1_2kHz() {
  cli();
  // CTC: OCR1A define a taxa
  // tick = 16MHz / 8 = 2MHz (0,5us); período 500us -> 1000 ticks -> OCR1A=999
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B |= (1 << WGM12);            // CTC
  TCCR1B |= (1 << CS11);             // prescaler 8
  OCR1A = (uint16_t)((F_CPU_HZ / (TIMER1_PRESC * FS_HZ)) - 1); // 999 para 2 kHz
  TIMSK1 |= (1 << OCIE1A);           // enable compare A interrupt
  sei();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // ADC default já entrega ~9-10 kSPS; 2 kHz é tranquilo.
  analogReference(DEFAULT); // 5V
  pinMode(EMG_PIN, INPUT);

  buildSinLUT();
  resetMAV1();
  resetMAVN((uint16_t)roundf((float)FS_HZ / NET_FREQ_INIT));

  // Inicializa NCO
  phase = 0;
  ph_inc = (uint32_t)(f_hat * NCO_K);

  setupTimer1_2kHz();

  Serial.println(F("# t(ms),x,y,f_hat"));
}

void loop() {
  static uint32_t t0 = millis();
  if (sampleReady) {
    noInterrupts();
    int16_t x = x_n;
    int16_t y = y_n;
    float   f = f_hat;
    sampleReady = false;
    interrupts();

    uint32_t t = millis() - t0;
    Serial.print(t); Serial.print(",");
    Serial.print(x); Serial.print(",");
    Serial.print(y); Serial.print(",");
    Serial.println(f, 2);
  }
}
