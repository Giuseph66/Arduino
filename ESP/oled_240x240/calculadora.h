// CalcMinimal3ButtonsRoundBright.h
// ESP32 + GC9A01 (240x240) + TFT_eSPI
// Uma única linha de 3 botões, dentro da área circular, com cores vivas e texto ASCII + sombra

#ifndef CALC_MINIMAL_3BUTTONS_ROUND_BRIGHT_H
#define CALC_MINIMAL_3BUTTONS_ROUND_BRIGHT_H

#include <TFT_eSPI.h>
#include <math.h>

// ---------- Util ----------
static inline uint16_t RGB(uint8_t r,uint8_t g,uint8_t b){
  return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
}

// Paleta moderna
static const uint16_t COL_BG     = RGB(8,12,20);
static const uint16_t COL_FRAME  = RGB(60,70,85);
static const uint16_t COL_PANEL  = RGB(25,30,40);
static const uint16_t COL_TEXT   = RGB(255,255,255);
static const uint16_t COL_SHADOW = RGB(0,0,0);
static const uint16_t COL_ACCENT = RGB(0,150,255);

// Cores dos botões (mais modernas)
static const uint16_t COL_BTN0 = RGB(255,100,50);   // laranja moderno
static const uint16_t COL_BTN1 = RGB(50,200,100);   // verde moderno
static const uint16_t COL_BTN2 = RGB(100,150,255);  // azul moderno

// ---------- Estado ----------
struct Calc3RoundBrightState {
  String currentExpr = "";   // expressão atual
  String currentResult = "0"; // resultado atual
  
  // Histórico de operações (como na imagem)
  String history[6] = {"", "", "", "", "", ""};  // últimas 6 operações
  int historyCount = 0;
  
  // Botões
  const char* b0 = "CLEAR";
  const char* b1 = "=";
  const char* b2 = "HIST";
  int highlight = -1;   // 0..2, -1 nenhum

  // Cores dos botões
  uint16_t c0 = COL_BTN0;
  uint16_t c1 = COL_BTN1;
  uint16_t c2 = COL_BTN2;
};

// ---------- Helpers ----------
static String sanitizeASCII(const String& in){
  // Converte alguns símbolos Unicode comuns para ASCII, pois font 1 não renderiza Unicode
  String s; s.reserve(in.length());
  for (uint16_t i=0;i<in.length();++i){
    char c = in[i];
    // substituições simples
    if ((uint8_t)c == 0xC3 && i+1<in.length()){ // possíveis bytes UTF-8 (sinais)
      // vamos mapear alguns padrões usados
      unsigned char c2 = (unsigned char)in[i+1];
      // exemplos: × (C3 97?), − etc... mas melhor mapear por codepoint prático:
    }
    // mapeia manualmente por comparação
    if (c=='\xC3'){ // pode ser começo de UTF-8, tenta sequências conhecidas
      // olhamos próxima posição só para descartar; deixamos vazio
      continue;
    }
    // fallback rápido: troca caracteres problemáticos por equivalentes
    if ((unsigned char)c > 126){
      // alguns mapeamentos diretos que costumam aparecer:
      // '×' -> 'x', '÷' -> '/', '−' -> '-', '•' -> '*'
      // como não temos o codepoint, mapeamos por conteúdo do label antes de passar aqui (veja uso abaixo).
      continue;
    }
    s += c;
  }
  // Substituições textuais frequentes se vierem na string original:
  String out = in;
  out.replace("×","x");
  out.replace("÷","/");
  out.replace("−","-");
  out.replace("•","*");
  out.replace("–","-");   // en dash
  out.replace("—","-");   // em dash
  out.replace("×","x");   // reforço
  // remove possíveis remanescentes não ASCII
  String ascii; ascii.reserve(out.length());
  for (uint16_t i=0;i<out.length();++i){
    char ch = out[i];
    if ((unsigned char)ch >= 32 && (unsigned char)ch <= 126) ascii += ch;
  }
  return ascii.length()? ascii : String("?");
}

static void drawTextWithShadow(TFT_eSPI &tft, const String& txt, int cx, int cy, uint16_t fg, uint16_t bg){
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);     // Fonte 2 (mais legível)
  // sombra suave
  tft.setTextColor(COL_SHADOW, bg);
  tft.drawString(txt, cx+1, cy+1);
  // texto principal
  tft.setTextColor(fg, bg);
  tft.drawString(txt, cx, cy);
}

// Declarações das funções
static void drawCalculatorView(TFT_eSPI &tft, const Calc3RoundBrightState &s);
static void drawHistoryView(TFT_eSPI &tft, const Calc3RoundBrightState &s);

static void drawTopDisplay(TFT_eSPI &tft, const Calc3RoundBrightState &s,
                           int x,int y,int w,int h){
  int r = 20;
  // Fundo com gradiente sutil
  tft.fillRoundRect(x,y,w,h,r,COL_PANEL);
  tft.drawRoundRect(x,y,w,h,r,COL_FRAME);
  
  // Linha decorativa no topo
  tft.drawLine(x+8, y+2, x+w-8, y+2, COL_ACCENT);

  // expressão (menor, topo-esquerda)
  String expr = sanitizeASCII(s.currentExpr);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(RGB(150,160,180), COL_PANEL);
  tft.drawString(expr.length()? expr : " ", x+12, y+12);

  // resultado (grande, centralizado) com sombra
  String res = sanitizeASCII(s.currentResult.length()? s.currentResult : "0");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  if (res.length() > 8) tft.setTextFont(2);
  drawTextWithShadow(tft, res, x+w/2, y+h/2+8, COL_TEXT, COL_PANEL);
}

static int chordWidthAtY(int cx,int cy,int R,int y){
  float dy = (float)y - (float)cy;
  float inside = (float)R*(float)R - dy*dy;
  if (inside <= 0) return 0;
  return (int)floorf(2.0f * sqrtf(inside));
}

static void drawRoundedButton(TFT_eSPI &tft, int x,int y,int w,int h,
                              const String& label, uint16_t fill, bool highlight){
  int r = 18;
  uint16_t bg = fill;
  uint16_t border = COL_FRAME;
  
  if (highlight){
    // Efeito de brilho mais sutil
    uint8_t R = min(255, (int)( (fill>>8)&0xF8 ) + 20);
    uint8_t G = min(255, (int)( (fill>>3)&0xFC ) + 20);
    uint8_t B = min(255, (int)( (fill<<3)&0xF8 ) + 20);
    bg = RGB(R,G,B);
    border = COL_ACCENT;
  }

  // Sombra do botão
  tft.fillRoundRect(x+2, y+2, w, h, r, RGB(0,0,0));
  // Botão principal
  tft.fillRoundRect(x,y,w,h,r,bg);
  tft.drawRoundRect(x,y,w,h,r,border);
  
  // Linha decorativa no topo do botão
  tft.drawLine(x+4, y+2, x+w-4, y+2, RGB(255,255,255));

  String safe = sanitizeASCII(label);
  drawTextWithShadow(tft, safe, x + w/2, y + h/2, COL_TEXT, bg);
}

// ---------- Função principal ----------
static void drawCalculator3ButtonsRoundBright(TFT_eSPI &tft, const Calc3RoundBrightState &s){
  const int CX=120, CY=120;
  const int R=110;

  tft.fillScreen(COL_BG);
  tft.drawCircle(CX, CY, R, COL_FRAME);

  // Sempre mostra a calculadora normal
  drawCalculatorView(tft, s);
}

static void drawCalculatorView(TFT_eSPI &tft, const Calc3RoundBrightState &s){
  const int CX=120, CY=120;
  
  // Área do histórico (otimizada para tela redonda)
  int histW = 180;
  int histH = 100;
  int histX = CX - histW/2;
  int histY = CY - 50;
  
  // Fundo do histórico
  tft.fillRoundRect(histX, histY, histW, histH, 8, COL_PANEL);
  tft.drawRoundRect(histX, histY, histW, histH, 8, COL_FRAME);
  
  // Linhas separadoras (menos linhas para caber melhor)
  for (int i = 1; i < 5; i++) {
    int lineY = histY + 18 + i * 16;
    tft.drawLine(histX + 10, lineY, histX + histW - 10, lineY, RGB(80,80,90));
  }
  
  // Mostra o histórico (últimas 5 operações)
  for (int i = 0; i < 5; i++) {
    int idx = (s.historyCount - 1 - i + 6) % 6;
    if (s.history[idx].length() > 0) {
      tft.setTextDatum(TL_DATUM);
      tft.setTextFont(2);
      tft.setTextColor(COL_TEXT, COL_PANEL);
      tft.drawString(s.history[idx], histX + 15, histY + 8 + i * 16);
    }
  }
  
  // Expressão atual no topo
  if (s.currentExpr.length() > 0) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(s.currentExpr, histX + 15, histY + 8);
  }

  // 3 botões na parte inferior (mais próximos)
  int btnW = 45;
  int btnH = 30;
  int btnY = CY + 60;
  int gap = 12;
  
  int totalW = 3*btnW + 2*gap;
  int startX = CX - totalW/2;
  
  const char* labels[] = {s.b0, s.b1, s.b2};
  uint16_t colors[] = {s.c0, s.c1, s.c2};
  bool highlights[] = {s.highlight==0, s.highlight==1, s.highlight==2};
  
  for (int i = 0; i < 3; i++) {
    int btnX = startX + i*(btnW + gap);
    uint16_t btnColor = colors[i];
    
    if (highlights[i]) {
      uint8_t R = min(255, (int)( (btnColor>>8)&0xF8 ) + 40);
      uint8_t G = min(255, (int)( (btnColor>>3)&0xFC ) + 40);
      uint8_t B = min(255, (int)( (btnColor<<3)&0xF8 ) + 40);
      btnColor = RGB(R,G,B);
    }
    
    tft.fillRoundRect(btnX, btnY, btnW, btnH, 6, btnColor);
    tft.drawRoundRect(btnX, btnY, btnW, btnH, 6, highlights[i] ? COL_ACCENT : COL_FRAME);
    
    String safe = sanitizeASCII(labels[i]);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, btnColor);
    tft.drawString(safe, btnX + btnW/2, btnY + btnH/2);
  }
}

static void drawHistoryView(TFT_eSPI &tft, const Calc3RoundBrightState &s){
  const int CX=120, CY=120;
  
  // Título
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.drawString("HISTORICO", CX, CY - 80);
  
  // Lista do histórico
  int startY = CY - 50;
  int lineH = 20;
  
  for (int i = 0; i < 5; i++) {
    int idx = (s.historyCount - 1 - i + 5) % 5;
    if (s.history[idx].length() > 0) {
      tft.setTextDatum(TL_DATUM);
      tft.setTextFont(2);
      tft.setTextColor(COL_TEXT, COL_BG);
      tft.drawString(s.history[idx], 20, startY + i * lineH);
    }
  }
  
  // Botão voltar
  int btnW = 80;
  int btnH = 30;
  int btnX = CX - btnW/2;
  int btnY = CY + 60;
  
  tft.fillRoundRect(btnX, btnY, btnW, btnH, 6, COL_BTN1);
  tft.drawRoundRect(btnX, btnY, btnW, btnH, 6, COL_FRAME);
  
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BTN1);
  tft.drawString("VOLTAR", CX, btnY + btnH/2);
}

// Função auxiliar para adicionar ao histórico
static void addToHistory(Calc3RoundBrightState &s, const String &entry) {
  // Move histórico para frente
  for (int i = 5; i > 0; i--) {
    s.history[i] = s.history[i-1];
  }
  s.history[0] = entry;
  s.historyCount = min(6, s.historyCount + 1);
}

// Redesenha a calculadora
static void redrawCalc3DisplayRoundBright(TFT_eSPI &tft, const Calc3RoundBrightState &s){
  drawCalculatorView(tft, s);
}

#endif // CALC_MINIMAL_3BUTTONS_ROUND_BRIGHT_H
