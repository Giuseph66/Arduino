#ifndef WATCH_FACE_H
#define WATCH_FACE_H

#include <TFT_eSPI.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

// Conversor rápido RGB888 -> RGB565
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ---------- Desenho de arcos “grossos” (com cantos arredondados) ----------
static void drawThickArc(TFT_eSPI &tft, int cx, int cy, int r,
                         int thickness, float a0_deg, float a1_deg,
                         uint16_t color, int step_deg = 1) {
  // Garante ordem
  if (a1_deg < a0_deg) { float t = a0_deg; a0_deg = a1_deg; a1_deg = t; }

  int rad = thickness / 2;
  for (int a = (int)a0_deg; a <= (int)a1_deg; a += step_deg) {
    float ar = a * (PI / 180.0f);
    int x = cx + (int)roundf(cosf(ar) * r);
    int y = cy + (int)roundf(sinf(ar) * r);
    tft.fillCircle(x, y, rad, color);
  }
}

// ----------------------------- WATCHFACE V3 (barras curvas) -----------------------------
struct WatchFaceV3Data {
  int hour;
  int minute;
  int second;
  int heart;              // bpm
  int sys;                // pressão sistólica
  int dia;                // pressão diastólica
  float heartProgress;    // 0..1
  float pressureProgress; // 0..1
  int battery;            // 0..100
  const char* date;       // "02/07"
  int year;               // 2025
};

// Desenha um rótulo na posição polar (ângulo/raio) com deslocamento em X
static void drawLabelAtAngle(TFT_eSPI &tft, int cx, int cy, float angleDeg,
                             int radius, int dx, uint16_t fg, uint16_t bg,
                             const char* text) {
  float ar = angleDeg * (PI / 180.0f);
  int x = cx + (int)roundf(cosf(ar) * radius) + dx;
  int y = cy + (int)roundf(sinf(ar) * radius);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(text, x, y);
}

static void drawBaseLabels(TFT_eSPI &tft, int cx, int cy, int radius,
                           int dxLeft, int dxRight,
                           int heart, int sys, int dia,
                           uint16_t colLeft, uint16_t colRight, uint16_t bg) {
  char hrBuf[16]; snprintf(hrBuf, sizeof(hrBuf), "%dbpm", heart);
  char bpBuf[20]; snprintf(bpBuf, sizeof(bpBuf), "%d/%d", sys, dia);
  drawLabelAtAngle(tft, cx, cy, 120.0f, radius, dxLeft,  colLeft,  bg, hrBuf);
  drawLabelAtAngle(tft, cx, cy,  60.0f, radius, dxRight, colRight, bg, bpBuf);
}

// ---------- Ícones simples (bateria, coração, chama, passos) ----------
static void drawBatteryIcon(TFT_eSPI &tft, int x, int y, int w, int h,
                            int percent, uint16_t fg, uint16_t bg, uint16_t frame) {
  int termW = w / 8;
  int innerW = w - termW - 3;
  // moldura
  tft.drawRect(x, y, w - termW, h, frame);
  // terminal
  tft.fillRect(x + (w - termW), y + (h/3), termW, h/3, frame);
  // fundo interno
  tft.fillRect(x + 2, y + 2, innerW - 4, h - 4, bg);
  // carga
  if (percent < 0) percent = 0; if (percent > 100) percent = 100;
  int fillW = (innerW - 4) * percent / 100;
  tft.fillRect(x + 2, y + 2, fillW, h - 4, fg);
}

// Desenha elementos comuns: bateria, rótulos, hora/data central
static void drawCommonElements(TFT_eSPI &tft, const WatchFaceV3Data &d) {
  const uint16_t COL_BG       = rgb565(5, 8, 12);
  const uint16_t COL_RING_BG  = rgb565(35, 40, 55);
  const uint16_t COL_WHITE    = rgb565(255, 255, 255);
  const uint16_t COL_SUB      = rgb565(170, 180, 190);
  const uint16_t COL_PINK_V   = rgb565(255, 30, 60);
  const uint16_t COL_ORANGE_V = rgb565(255, 140, 20);
  const uint16_t COL_CYAN_V   = rgb565(0, 255, 230);
  const uint16_t COL_TEXT     = rgb565(235, 240, 245);
  const int CX = 120, CY = 120;
  const int BAR_R = 96, BAR_THICK = 14;
  
  // Bateria centralizada
  int by = 6, bw = 34, bh = 14;
  char batBuf[8]; snprintf(batBuf, sizeof(batBuf), "%d%%", d.battery);
  tft.setTextFont(2);
  int tw = tft.textWidth(batBuf, 2);
  int totalW = bw + 6 + tw;
  int bx = CX - totalW/2;
  drawBatteryIcon(tft, bx, by, bw, bh, d.battery, COL_WHITE, COL_RING_BG, COL_SUB);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_SUB, COL_BG);
  int textY = by + (bh - 16)/2 + 1;
  tft.drawString(batBuf, bx + bw + 6, textY);
  
  // Rótulos das barras
  int labelRadius = BAR_R + BAR_THICK/2 + 12;
  drawBaseLabels(tft, CX, CY, labelRadius, +30, -30,
                 d.heart, d.sys, d.dia, COL_PINK_V, COL_ORANGE_V, COL_BG);
  
  // Hora/data central
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_CYAN_V, COL_BG);
  tft.setTextFont(4);
  char timeBuf[16]; snprintf(timeBuf, sizeof(timeBuf), "---%02d:%02d---", d.hour, d.minute);
  tft.drawString(timeBuf, CX, CY - 12);
  
  int dd = 0, mm = 0;
  if (d.date) {
    if (sscanf(d.date, "%d/%d", &dd, &mm) != 2) {
      sscanf(d.date, "%d:%d", &dd, &mm);
    }
  }
  tft.setTextFont(2);
  tft.setTextColor(COL_SUB, COL_BG);
  char dateBuf[20]; snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d/%04d", dd, mm, d.year);
  tft.drawString(dateBuf, CX, CY + 18);
}

// Fundo + progresso sobreposto (ex.: barra de passos, batimentos)
static void drawArcProgress(TFT_eSPI &tft, int cx, int cy, int r, int thickness,
                            float start_deg, float end_deg, float progress01,
                            uint16_t bg, uint16_t fg) {
  if (progress01 < 0) progress01 = 0;
  if (progress01 > 1) progress01 = 1;

  drawThickArc(tft, cx, cy, r, thickness, start_deg, end_deg, bg);
  float span = (end_deg - start_deg) * progress01;
  drawThickArc(tft, cx, cy, r, thickness, start_deg, start_deg + span, fg);
}

static void drawHeartIcon(TFT_eSPI &tft, int cx, int cy, int r, uint16_t col) {
  // coração aproximado com 2 círculos + triângulo
  tft.fillCircle(cx - r/2, cy - r/2, r/2, col);
  tft.fillCircle(cx + r/2, cy - r/2, r/2, col);
  tft.fillTriangle(cx - r, cy - r/3, cx + r, cy - r/3, cx, cy + r, col);
}

static void drawFlameIcon(TFT_eSPI &tft, int cx, int cy, int r, uint16_t colOuter, uint16_t colInner) {
  // gota dupla
  tft.fillCircle(cx, cy, r, colOuter);
  tft.fillTriangle(cx, cy - r, cx - r/2, cy + r/3, cx + r/2, cy + r/3, colOuter);
  tft.fillCircle(cx, cy + r/6, r/2, colInner);
  tft.fillTriangle(cx, cy - r/2, cx - r/4, cy + r/4, cx + r/4, cy + r/4, colInner);
}

static void drawStepsIcon(TFT_eSPI &tft, int cx, int cy, uint16_t col) {
  // dois "pés" minimalistas
  tft.fillCircle(cx - 6, cy, 4, col);
  tft.fillCircle(cx + 4, cy + 6, 4, col);
  tft.fillCircle(cx - 9, cy - 6, 3, col);
  tft.fillCircle(cx + 7, cy + 12, 3, col);
}

// ---------- Função principal da face ----------
struct WatchFaceData {
  int    hour;       // 0-23
  int    minute;     // 0-59
  int    steps;      // 5084
  int    heart;      // 133 bpm
  int    kcal;       // 418
  int    battery;    // 0-100
  const char* date;  // "02/07"
  const char* wday;  // "Tuesday"
  // progresso 0..1 para os arcos laterais
  float  stepsProgress; // por ex.: steps/meta
  float  heartProgress; // por ex.: heart/200
};

static void drawWatchFace(TFT_eSPI &tft, const WatchFaceData &d) {
  // Paleta semelhante à imagem
  const uint16_t COL_BG      = rgb565(10, 10, 14);
  const uint16_t COL_RING_BG = rgb565(40, 40, 50);
  const uint16_t COL_CYAN    = rgb565(0, 210, 190);
  const uint16_t COL_CYAN_D  = rgb565(0, 140, 130);
  const uint16_t COL_ORANGE  = rgb565(245, 100, 20);
  const uint16_t COL_RED     = rgb565(230, 40, 60);
  const uint16_t COL_WHITE   = rgb565(255, 255, 255);
  const uint16_t COL_GREY    = rgb565(160, 165, 175);
  const uint16_t COL_TEXT    = rgb565(220, 230, 235);
  const uint16_t COL_SUB     = rgb565(150, 160, 170);

  const int CX = 120, CY = 120; // centro 240x240
  tft.fillScreen(COL_BG);

  // Borda de referência
  tft.drawCircle(CX, CY, 118, COL_RING_BG);

  // Arco de passos (esquerda): ~210° -> 330°
  drawArcProgress(tft, CX, CY, 96, 16, 210, 330, d.stepsProgress, COL_RING_BG, COL_CYAN);
  // Toque de “acabamento” (cantos mais vivos)
  drawArcProgress(tft, CX, CY, 96, 6, 210, 330, 1.0f, rgb565(0,80,80), rgb565(0,80,80));

  // Arco de batimentos (direita): ~30° -> 150°
  drawArcProgress(tft, CX, CY, 96, 16, 30, 150, d.heartProgress, COL_RING_BG, COL_ORANGE);
  drawArcProgress(tft, CX, CY, 96, 6, 30, 150, 1.0f, rgb565(100,40,0), rgb565(100,40,0));

  // --------- Hora grande ----------
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", d.hour, d.minute);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(7); // dígitos bem grandes (7-seg estilizado)
  tft.drawString(timeBuf, CX, CY - 12);

  // --------- Data e dia ----------
  tft.setTextFont(2);
  tft.setTextColor(COL_SUB, COL_BG);
  tft.setTextDatum(MR_DATUM); // direita para “02/07”
  tft.drawString(d.date, CX - 18, CY + 38);
  tft.setTextDatum(ML_DATUM); // esquerda para “Tuesday”
  tft.drawString(d.wday, CX + 18, CY + 38);

  // --------- Bateria no topo ----------
  int bx = CX - 28, by = 14;
  drawBatteryIcon(tft, bx, by, 42, 16, d.battery, COL_WHITE, COL_RING_BG, COL_GREY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_SUB, COL_BG);
  char batBuf[8]; snprintf(batBuf, sizeof(batBuf), "%d%%", d.battery);
  tft.drawString(batBuf, bx + 48, by + 2);

  // --------- Passos (esquerda) ----------
  drawStepsIcon(tft, 44, 86, COL_CYAN_D);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_CYAN, COL_BG);
  tft.setTextFont(4);
  char stepBuf[12]; snprintf(stepBuf, sizeof(stepBuf), "%d", d.steps);
  tft.drawString(stepBuf, 60, 78);

  // --------- Batimentos (direita) ----------
  drawHeartIcon(tft, 196, 86, 16, COL_RED);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(COL_ORANGE, COL_BG);
  tft.setTextFont(4);
  char hrBuf[12]; snprintf(hrBuf, sizeof(hrBuf), "%d", d.heart);
  tft.drawString(hrBuf, 180, 78);

  // --------- Kcal (base) ----------
  drawFlameIcon(tft, CX - 40, CY + 86, 10, COL_ORANGE, rgb565(255, 180, 80));
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);
  char kcBuf[16]; snprintf(kcBuf, sizeof(kcBuf), "%dkcal", d.kcal);
  tft.drawString(kcBuf, CX - 26, CY + 80);
}

// ----------------------------- NOVO WATCHFACE -----------------------------
struct WatchFaceV2Data {
  int hour;
  int minute;
  int second;
  int heart;          // bpm
  int sys;            // pressão sistólica
  int dia;            // pressão diastólica
  float heartProgress;    // 0..1
  float pressureProgress; // 0..1
  int battery;        // 0..100
  const char* date;   // "02/07"
};

static void drawVerticalBar(TFT_eSPI &tft, int x, int y, int w, int h,
                            float progress01, uint16_t frame, uint16_t bg, uint16_t fg) {
  if (progress01 < 0) progress01 = 0; if (progress01 > 1) progress01 = 1;
  tft.drawRoundRect(x, y, w, h, 4, frame);
  tft.fillRoundRect(x + 1, y + 1, w - 2, h - 2, 3, bg);
  int fillH = (int)((h - 4) * progress01);
  if (fillH > 0) tft.fillRoundRect(x + 2, y + h - 2 - fillH, w - 4, fillH, 2, fg);
}

static void drawAnalogDial(TFT_eSPI &tft, int cx, int cy, int r,
                           uint16_t ring, uint16_t ticks, uint16_t bg) {
  tft.fillCircle(cx, cy, r + 2, bg);
  tft.drawCircle(cx, cy, r, ring);
  // marcas de hora (12)
  for (int i = 0; i < 12; i++) {
    float a = (i * 30) * (PI / 180.0f);
    int x0 = cx + (int)roundf(cosf(a) * (r - 10));
    int y0 = cy + (int)roundf(sinf(a) * (r - 10));
    int x1 = cx + (int)roundf(cosf(a) * (r - 2));
    int y1 = cy + (int)roundf(sinf(a) * (r - 2));
    tft.drawLine(x0, y0, x1, y1, ticks);
  }
  // marcas de minuto (a cada 5 já temos as horas; desenhe as menores entre elas)
  for (int i = 0; i < 60; i++) {
    if (i % 5 == 0) continue;
    float a = (i * 6) * (PI / 180.0f);
    int x0 = cx + (int)roundf(cosf(a) * (r - 6));
    int y0 = cy + (int)roundf(sinf(a) * (r - 6));
    int x1 = cx + (int)roundf(cosf(a) * (r - 2));
    int y1 = cy + (int)roundf(sinf(a) * (r - 2));
    tft.drawLine(x0, y0, x1, y1, ticks);
  }
}

static void drawAnalogHands(TFT_eSPI &tft, int cx, int cy, int r,
                            int hour, int minute, int second,
                            uint16_t colH, uint16_t colM, uint16_t colS) {
  float aH = ((hour % 12) * 30 + minute * 0.5f) * (PI / 180.0f);
  float aM = (minute * 6) * (PI / 180.0f);
  float aS = (second * 6) * (PI / 180.0f);

  int hx = cx + (int)roundf(cosf(aH) * (r * 0.55f));
  int hy = cy + (int)roundf(sinf(aH) * (r * 0.55f));
  int mx = cx + (int)roundf(cosf(aM) * (r * 0.80f));
  int my = cy + (int)roundf(sinf(aM) * (r * 0.80f));
  int sx = cx + (int)roundf(cosf(aS) * (r * 0.90f));
  int sy = cy + (int)roundf(sinf(aS) * (r * 0.90f));

  tft.drawLine(cx, cy, hx, hy, colH);
  tft.drawLine(cx, cy, mx, my, colM);
  tft.drawLine(cx, cy, sx, sy, colS);
  tft.fillCircle(cx, cy, 3, colM);
}

static void drawWatchFaceV2(TFT_eSPI &tft, const WatchFaceV2Data &d) {
  // Paleta
  const uint16_t COL_BG      = rgb565(10, 10, 14);
  const uint16_t COL_RING_BG = rgb565(40, 40, 50);
  const uint16_t COL_CYAN    = rgb565(0, 210, 190);
  const uint16_t COL_ORANGE  = rgb565(245, 120, 40);
  const uint16_t COL_WHITE   = rgb565(255, 255, 255);
  const uint16_t COL_TEXT    = rgb565(220, 230, 235);
  const uint16_t COL_SUB     = rgb565(150, 160, 170);
  const uint16_t COL_RED     = rgb565(230, 40, 60);

  const int CX = 120, CY = 120;
  tft.fillScreen(COL_BG);

  // Bateria no topo (centralizada, menor, com % centralizada)
  int by = 6;
  int bw = 34, bh = 14;
  char batBuf[8]; snprintf(batBuf, sizeof(batBuf), "%d%%", d.battery);
  tft.setTextFont(2);
  int tw = tft.textWidth(batBuf, 2);
  int totalW = bw + 6 + tw;
  int bx = CX - totalW/2;
  drawBatteryIcon(tft, bx, by, bw, bh, d.battery, COL_WHITE, COL_RING_BG, COL_SUB);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_SUB, COL_BG);
  int textY = by + (bh - 16)/2 + 1; // altura aprox da fonte 2
  tft.drawString(batBuf, bx + bw + 6, textY);

  // Barras laterais (esquerda: HR, direita: BP)
  const int barY = 36;
  const int barH = 168;
  const int barW = 18;

  // HR (esquerda)
  int leftX = 10;
  drawVerticalBar(tft, leftX, barY, barW, barH, d.heartProgress, COL_SUB, COL_RING_BG, COL_CYAN);
  // ícone + texto
  drawHeartIcon(tft, leftX + barW/2, barY - 8, 8, COL_RED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_CYAN, COL_BG);
  char hrBuf[16]; snprintf(hrBuf, sizeof(hrBuf), "%dbpm", d.heart);
  tft.drawString(hrBuf, leftX + barW + 6, barY + barH - 14);

  // BP (direita)
  int rightX = 240 - 10 - barW;
  drawVerticalBar(tft, rightX, barY, barW, barH, d.pressureProgress, COL_SUB, COL_RING_BG, COL_ORANGE);
  tft.setTextDatum(MR_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_ORANGE, COL_BG);
  char bpBuf[20]; snprintf(bpBuf, sizeof(bpBuf), "%d/%d", d.sys, d.dia);
  tft.drawString(bpBuf, rightX - 6, barY + barH - 14);

  // Mostrador analógico ao centro
  const int dialR = 78;
  drawAnalogDial(tft, CX, CY - 6, dialR, COL_RING_BG, COL_SUB, COL_BG);
  drawAnalogHands(tft, CX, CY - 6, dialR, d.hour, d.minute, d.second,
                  COL_TEXT, COL_WHITE, COL_RED);

  // Digital + data abaixo do mostrador
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(4);
  char timeBuf[10]; snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", d.hour, d.minute, d.second);
  tft.drawString(timeBuf, CX, CY + 76);

  tft.setTextFont(2);
  tft.setTextColor(COL_SUB, COL_BG);
  tft.drawString(d.date, CX, CY + 96);
}

static void drawWatchFaceV3(TFT_eSPI &tft, const WatchFaceV3Data &d) {
  // Paleta mais viva
  const uint16_t COL_BG       = rgb565(5, 8, 12);
  const uint16_t COL_RING_BG  = rgb565(35, 40, 55);
  const uint16_t COL_CYAN_V   = rgb565(0, 255, 230);
  const uint16_t COL_PINK_V   = rgb565(255, 30, 60);
  const uint16_t COL_ORANGE_V = rgb565(255, 140, 20);
  const uint16_t COL_LIME_V   = rgb565(120, 255, 80);
  const uint16_t COL_WHITE    = rgb565(255, 255, 255);
  const uint16_t COL_TEXT     = rgb565(235, 240, 245);
  const uint16_t COL_SUB      = rgb565(170, 180, 190);
  const uint16_t COL_RED      = rgb565(255, 30, 60);

  const int CX = 120, CY = 120;
  tft.fillScreen(COL_BG);

  // Barras arredondadas nas LATERAIS
  const int BAR_R = 96, BAR_THICK = 14;
  // esquerda (fundo completo)
  drawArcProgress(tft, CX, CY, BAR_R, BAR_THICK, 120, 240, 1.0f, COL_RING_BG, COL_RING_BG);
  // progresso crescendo de baixo (240°) para cima (120°)
  {
    float nh = d.heartProgress; if (nh < 0) nh = 0; if (nh > 1) nh = 1;
    float ca = 240.0f - (240.0f - 120.0f) * nh; // ângulo atual
    drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, ca, 240.0f, COL_PINK_V);
  }
  // direita (fundo completo)
  drawArcProgress(tft, CX, CY, BAR_R, BAR_THICK, 300, 420, 1.0f, COL_RING_BG, COL_RING_BG);
  // progresso crescendo de baixo (300°) para cima (420°)
  {
    float np = d.pressureProgress; if (np < 0) np = 0; if (np > 1) np = 1;
    float ca = 300.0f + (420.0f - 300.0f) * np;
    drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, 300.0f, ca, COL_ORANGE_V);
  }

  // Mostrador analógico (centralizado)
  const int dialR = 74;
  drawAnalogDial(tft, CX, CY, dialR, COL_LIME_V, COL_SUB, COL_BG);
  drawAnalogHands(tft, CX, CY, dialR, d.hour, d.minute, d.second,
                  COL_TEXT, COL_WHITE, COL_RED);

  // Elementos comuns (bateria, rótulos, hora/data)
  drawCommonElements(tft, d);
}

// Desenho sem flicker: base estática + atualização parcial com sprites
static void drawWatchFaceV3Static(TFT_eSPI &tft, const WatchFaceV3Data &d) {
  const uint16_t COL_BG       = rgb565(5, 8, 12);
  const uint16_t COL_RING_BG  = rgb565(35, 40, 55);
  const uint16_t COL_WHITE    = rgb565(255, 255, 255);
  const uint16_t COL_SUB      = rgb565(170, 180, 190);
  const uint16_t COL_PINK_V   = rgb565(255, 30, 60);
  const uint16_t COL_ORANGE_V = rgb565(255, 140, 20);
  const int CX = 120, CY = 120;
  tft.fillScreen(COL_BG);
  const int BAR_R = 96, BAR_THICK = 14;
  drawArcProgress(tft, CX, CY, BAR_R, BAR_THICK, 120, 240, 1.0f, COL_RING_BG, COL_RING_BG);
  drawArcProgress(tft, CX, CY, BAR_R, BAR_THICK, 300, 420, 1.0f, COL_RING_BG, COL_RING_BG);
  
  // Elementos comuns (bateria, rótulos, hora/data)
  drawCommonElements(tft, d);
}

static void drawAnalogDialSprite(TFT_eSprite &sp, int cx, int cy, int r,
                                 uint16_t ring, uint16_t ticks, uint16_t bg) {
  sp.fillCircle(cx, cy, r + 2, bg);
  sp.drawCircle(cx, cy, r, ring);
  for (int i = 0; i < 12; i++) {
    float a = (i * 30) * (PI / 180.0f);
    int x0 = cx + (int)roundf(cosf(a) * (r - 10));
    int y0 = cy + (int)roundf(sinf(a) * (r - 10));
    int x1 = cx + (int)roundf(cosf(a) * (r - 2));
    int y1 = cy + (int)roundf(sinf(a) * (r - 2));
    sp.drawLine(x0, y0, x1, y1, ticks);
  }
  for (int i = 0; i < 60; i++) {
    if (i % 5 == 0) continue;
    float a = (i * 6) * (PI / 180.0f);
    int x0 = cx + (int)roundf(cosf(a) * (r - 6));
    int y0 = cy + (int)roundf(sinf(a) * (r - 6));
    int x1 = cx + (int)roundf(cosf(a) * (r - 2));
    int y1 = cy + (int)roundf(sinf(a) * (r - 2));
    sp.drawLine(x0, y0, x1, y1, ticks);
  }
}

static void drawAnalogHandsSprite(TFT_eSprite &sp, int cx, int cy, int r,
                                  int hour, int minute, int second,
                                  uint16_t colH, uint16_t colM, uint16_t colS) {
  float aH = ((hour % 12) * 30 + minute * 0.5f) * (PI / 180.0f);
  float aM = (minute * 6) * (PI / 180.0f);
  float aS = (second * 6) * (PI / 180.0f);
  int hx = cx + (int)roundf(cosf(aH) * (r * 0.55f));
  int hy = cy + (int)roundf(sinf(aH) * (r * 0.55f));
  int mx = cx + (int)roundf(cosf(aM) * (r * 0.80f));
  int my = cy + (int)roundf(sinf(aM) * (r * 0.80f));
  int sx = cx + (int)roundf(cosf(aS) * (r * 0.90f));
  int sy = cy + (int)roundf(sinf(aS) * (r * 0.90f));
  sp.drawLine(cx, cy, hx, hy, colH);
  sp.drawLine(cx, cy, mx, my, colM);
  sp.drawLine(cx, cy, sx, sy, colS);
  sp.fillCircle(cx, cy, 3, colM);
}

static void drawWatchFaceV3Update(TFT_eSPI &tft, const WatchFaceV3Data &d) {
  const uint16_t COL_BG       = rgb565(5, 8, 12);
  const uint16_t COL_RING_BG  = rgb565(35, 40, 55);
  const uint16_t COL_PINK_V   = rgb565(255, 30, 60);
  const uint16_t COL_ORANGE_V = rgb565(255, 140, 20);
  const uint16_t COL_LIME_V   = rgb565(120, 255, 80);
  const uint16_t COL_WHITE    = rgb565(255, 255, 255);
  const uint16_t COL_TEXT     = rgb565(235, 240, 245);
  const uint16_t COL_CYAN_V   = rgb565(0, 255, 230);
  const uint16_t COL_RED      = rgb565(255, 30, 60);
  const int CX = 120, CY = 120;
  // Atualização incremental das barras (sem piscar): só o delta
  const int BAR_R = 96;
  const int BAR_THICK = 14;
  static float prevHeart = -1.0f;
  static float prevPress = -1.0f;
  float nh = d.heartProgress; if (nh < 0) nh = 0; if (nh > 1) nh = 1;
  float np = d.pressureProgress; if (np < 0) np = 0; if (np > 1) np = 1;

  tft.startWrite();
  // Esquerda: base 240° subindo até 120°
  float baseL = 240.0f, topL = 120.0f;
  float caL = baseL - (baseL - topL) * nh;
  if (prevHeart < 0) {
    drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, caL, baseL, COL_PINK_V);
  } else if (nh != prevHeart) {
    float prevCaL = baseL - (baseL - topL) * prevHeart;
    if (nh > prevHeart) {
      // cresceu: desenha novo trecho de caL até prevCaL
      drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, caL, prevCaL, COL_PINK_V);
    } else {
      // diminuiu: apaga trecho de prevCaL até caL
      drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, prevCaL, caL, COL_RING_BG);
    }
  }
  prevHeart = nh;

  // Direita: base 300° subindo até 420°
  float baseR = 300.0f, topR = 420.0f;
  float caR = baseR + (topR - baseR) * np;
  if (prevPress < 0) {
    drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, baseR, caR, COL_ORANGE_V);
  } else if (np != prevPress) {
    float prevCaR = baseR + (topR - baseR) * prevPress;
    if (np > prevPress) {
      drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, prevCaR, caR, COL_ORANGE_V);
    } else {
      drawThickArc(tft, CX, CY, BAR_R, BAR_THICK, caR, prevCaR, COL_RING_BG);
    }
  }
  prevPress = np;
  tft.endWrite();
  // Redesenha rótulos com valores atuais
  {
    int labelRadius = BAR_R + BAR_THICK/2 + 12;
    drawBaseLabels(tft, CX, CY, labelRadius, +30, -30,
                   d.heart, d.sys, d.dia, COL_PINK_V, COL_ORANGE_V, COL_BG);
  }
  // Sprite do mostrador analógico
  static bool spInit = false;
  static TFT_eSprite spDial(&tft);
  static TFT_eSprite spBottom(&tft);
  if (!spInit) {
    // Dial com transparência por colorkey requer 16 bits
    spDial.setColorDepth(16);
    spDial.createSprite(160, 160);
    // Bottom também com transparência para evitar "bloco"
    spBottom.setColorDepth(16);
    spBottom.createSprite(200, 80); // caixa maior p/ evitar clip de fonte 4 + data
    spInit = true;
  }
  // dial centralizado, com transparência fora do círculo
  const uint16_t TRANSP = 0x0001; // cor-chave de transparência (não usada no desenho)
  spDial.fillSprite(TRANSP);
  drawAnalogDialSprite(spDial, 80, 80, 74, COL_LIME_V, COL_RING_BG, COL_BG);
  drawAnalogHandsSprite(spDial, 80, 80, 74, d.hour, d.minute, d.second, COL_TEXT, COL_WHITE, COL_RED);
  spDial.pushSprite(CX - 80, CY - 80, TRANSP);
  // Centro do dial: topo HH:MM e base DD:MM:AAAA
  spBottom.fillSprite(TRANSP);
  spBottom.setTextDatum(MC_DATUM);
  spBottom.setTextColor(COL_CYAN_V);
  spBottom.setTextFont(4);
  char timeBuf2[16]; snprintf(timeBuf2, sizeof(timeBuf2), "---%02d:%02d---", d.hour, d.minute);
  spBottom.drawString(timeBuf2, 100, 18);
  int dd2 = 0, mm2 = 0;
  if (d.date) {
    if (sscanf(d.date, "%d/%d", &dd2, &mm2) != 2) {
      sscanf(d.date, "%d:%d", &dd2, &mm2);
    }
  }
  spBottom.setTextFont(2);
  spBottom.setTextColor(COL_TEXT);
  char dateBuf2[20]; snprintf(dateBuf2, sizeof(dateBuf2), "%02d0/%02d/%04d", dd2, mm2, d.year);
  spBottom.drawString(dateBuf2, 100, 44);
  // posiciona central ao dial
  spBottom.pushSprite(CX - 100, CY - 30, TRANSP);
}

#endif // WATCH_FACE_H
