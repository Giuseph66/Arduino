// NotifyUI.h
// ESP32 + GC9A01 (240x240 round) + TFT_eSPI
// Cartão de notificação (toast) dentro da área circular, com wrap de texto.

#ifndef NOTIFY_UI_H
#define NOTIFY_UI_H

#include <TFT_eSPI.h>
#include <math.h>

// ---------- Util ----------
static inline uint16_t RGB(uint8_t r,uint8_t g,uint8_t b){
  return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
}
static String asciiOnly(String s){
  s.replace("á","a"); s.replace("é","e"); s.replace("í","i");
  s.replace("ó","o"); s.replace("ú","u"); s.replace("ç","c");
  s.replace("Á","A"); s.replace("É","E"); s.replace("Í","I");
  s.replace("Ó","O"); s.replace("Ú","U"); s.replace("Ç","C");
  s.replace("ã","a"); s.replace("õ","o"); s.replace("Ã","A"); s.replace("Õ","O");
  s.replace("â","a"); s.replace("ê","e"); s.replace("ô","o"); s.replace("Â","A"); s.replace("Ê","E"); s.replace("Ô","O");
  s.replace("×","x"); s.replace("÷","/"); s.replace("−","-");
  // filtra não-ASCII
  String out; out.reserve(s.length());
  for (uint16_t i=0;i<s.length();++i){ char c=s[i]; if ((uint8_t)c>=32 && (uint8_t)c<=126) out+=c; }
  return out.length()? out : String("?");
}
static int chordWidthAtY(int cx,int cy,int R,int y){
  float dy = (float)y - (float)cy;
  float inside = (float)R*(float)R - dy*dy;
  if (inside <= 0) return 0;
  return (int)floorf(2.0f * sqrtf(inside));
}

// ---------- Dados da notificação ----------
struct NotifyCard {
  const char* app    = "App";
  const char* title  = "Contato";
  const char* body   = "Mensagem de exemplo chegando ao seu relogio.";
  const char* time   = "19:42";
  uint16_t accent    = RGB(0,210,255); // cor do app/icone
  bool unread        = true;
};

// ---------- Desenha ícones dos apps ----------
static void drawAppIcon(TFT_eSPI &tft, int cx, int cy, int r, const char* app, uint16_t color) {
  String appName = String(app);
  
  if (appName == "WhatsApp") {
    // Ícone WhatsApp - telefone com círculo
    tft.fillCircle(cx, cy, r, color);
    tft.fillCircle(cx, cy, r-2, RGB(255,255,255));
    // Telefone
    tft.fillRect(cx-4, cy-6, 8, 10, color);
    tft.fillRect(cx-2, cy-8, 4, 2, color);
    tft.fillRect(cx-1, cy+4, 2, 2, color);
    
  } else if (appName == "Instagram") {
    // Ícone Instagram - câmera
    tft.fillRoundRect(cx-r+2, cy-r+2, r*2-4, r*2-4, 4, color);
    tft.fillRoundRect(cx-r+4, cy-r+4, r*2-8, r*2-8, 2, RGB(255,255,255));
    // Lente
    tft.fillCircle(cx, cy, r-4, color);
    tft.fillCircle(cx, cy, r-6, RGB(255,255,255));
    tft.fillCircle(cx, cy, 2, color);
    
  } else if (appName == "Email") {
    // Ícone Email - envelope
    tft.fillRoundRect(cx-r+1, cy-r+3, r*2-2, r*2-6, 2, color);
    tft.fillRoundRect(cx-r+3, cy-r+1, r*2-6, r*2-2, 1, RGB(255,255,255));
    // Seta
    tft.fillTriangle(cx-r+2, cy-r+3, cx, cy-1, cx+r-2, cy-r+3, color);
    
  } else if (appName == "Telegram") {
    // Ícone Telegram - avião de papel
    tft.fillCircle(cx, cy, r, color);
    // Asas do avião
    tft.fillTriangle(cx-6, cy-2, cx+6, cy-2, cx, cy+4, RGB(255,255,255));
    tft.fillTriangle(cx-4, cy, cx+4, cy, cx, cy+2, color);
    
  } else if (appName == "SMS") {
    // Ícone SMS - balão de conversa
    tft.fillRoundRect(cx-r+2, cy-r+2, r*2-4, r*2-6, 3, color);
    tft.fillRoundRect(cx-r+4, cy-r+4, r*2-8, r*2-10, 1, RGB(255,255,255));
    // Cauda do balão
    tft.fillTriangle(cx-2, cy+r-4, cx+2, cy+r-4, cx, cy+r-1, color);
    
  } else {
    // Ícone padrão - círculo com inicial
    tft.fillCircle(cx, cy, r, color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(RGB(255,255,255), color);
    String initial = String(app[0]);
    tft.drawString(initial, cx, cy);
  }
}

// ---------- Texto com wrap em largura fixa ----------
static void drawWrapped(TFT_eSPI &tft, const String& text, int x, int y, int w, int lines, uint8_t textSize, uint16_t fg, uint16_t bg){
  tft.setTextFont(1);
  tft.setTextSize(textSize);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(TL_DATUM);

  String s = asciiOnly(text);
  int i = 0;
  int lineH = tft.fontHeight(1) * textSize + 2;
  for (int ln=0; ln<lines && i < s.length(); ++ln){
    int best = i, j = i+1, lastSpace = -1;
    while (j <= s.length()){
      String cand = s.substring(i, j);
      if (tft.textWidth(cand) > w) break;
      if (s[j-1] == ' ') lastSpace = j-1;
      best = j;
      j++;
    }
    int end = best;
    if (lastSpace > i && lastSpace < best) end = lastSpace;
    String line = s.substring(i, end);
    // trim fim
    while (line.length() && line[line.length()-1]==' ') line.remove(line.length()-1);
    if (!line.length()){ line = s.substring(i, min((int)s.length(), i+1)); end = i + line.length(); }
    tft.drawString(line, x, y + ln*lineH);
    i = (end < (int)s.length() && s[end]==' ') ? end+1 : end;
  }
}

// ---------- Cartão (toast) ----------
// yCenter define a altura do cartão; 88 é bom para topo/centro.
// actions: mostra "Desc  |  Abrir  |  Resp" (opcional)
static void drawNotifyToast(TFT_eSPI &tft, const NotifyCard &n, int yCenter = 88, bool actions=false){
  const int W=240,H=240,CX=120,CY=120;
  const int R=118;          // raio da borda desenhada
  const int SAFE=R-8;       // margem de segurança
  const uint16_t COL_BG     = RGB(12,14,18);
  const uint16_t COL_FRAME  = RGB(40,45,55);
  const uint16_t COL_CARD   = RGB(22,24,30);
  const uint16_t COL_TEXT   = RGB(235,240,245);
  const uint16_t COL_SUB    = RGB(160,170,185);

  // fundo e aro (deixe o fundo como já estiver se quiser sobrepor apenas o cartão)
  // tft.fillScreen(COL_BG);
  // tft.drawCircle(CX,CY,R,COL_FRAME);

  // tamanho do cartão pela corda do círculo nesta altura
  int cardH = actions ? 110 : 92;
  int yTop  = yCenter - cardH/2;
  int yBot  = yCenter + cardH/2;

  // largura = corda na base do cartão (para não vazar nas bordas)
  int chordTop = chordWidthAtY(CX, CY, SAFE, yTop+12);
  int chordMid = chordWidthAtY(CX, CY, SAFE, yCenter);
  int chordBot = chordWidthAtY(CX, CY, SAFE, yBot-12);
  int cardW = min(chordTop, min(chordMid, chordBot)) - 12; // folga
  if (cardW > 212) cardW = 212;
  if (cardW < 140) cardW = 140;
  int xLeft = CX - cardW/2;

  // cartão
  int radius = 16;
  tft.fillRoundRect(xLeft, yTop, cardW, cardH, radius, COL_CARD);
  tft.drawRoundRect(xLeft, yTop, cardW, cardH, radius, COL_FRAME);

  // ícone do app (desenhado) + app name
  int pad = 10;
  int iconR = 12;
  int iconX = xLeft + pad + iconR;
  int iconY = yTop + pad + iconR;
  
  // Desenha ícone baseado no app
  drawAppIcon(tft, iconX, iconY, iconR, n.app, n.accent);
  
  if (n.unread) tft.fillCircle(iconX + iconR-3, iconY - iconR+3, 3, RGB(255,80,120)); // badge

  // app (pequeno) à direita do ícone
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(COL_SUB, COL_CARD); tft.setTextDatum(TL_DATUM);
  tft.drawString(asciiOnly(n.app), iconX + iconR + 6, yTop + 6);

  // tempo (canto superior direito)
  tft.setTextDatum(TR_DATUM);
  tft.drawString(asciiOnly(n.time), xLeft + cardW - 8, yTop + 6);

  // Título (remetente) — maior
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2); tft.setTextColor(COL_TEXT, COL_CARD);
  tft.drawString(asciiOnly(n.title), xLeft + pad, yTop + 24);

  // Corpo com wrap (2–3 linhas)
  int textX = xLeft + pad;
  int textY = yTop + 24 + (tft.fontHeight(1)*2) + 4;
  int textW = cardW - 2*pad;
  drawWrapped(tft, n.body, textX, textY, textW, 3, 1, COL_TEXT, COL_CARD);

  if (actions){
    // barra de ações
    int barH = 26;
    int barY = yBot - barH;
    // separador superior
    tft.drawLine(xLeft+8, barY, xLeft+cardW-8, barY, COL_FRAME);
    const char* A0="Desc"; const char* A1="Abrir"; const char* A2="Resp";
    int w3 = cardW/3;
    tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(COL_SUB, COL_CARD); tft.setTextDatum(MC_DATUM);
    tft.drawString(A0, xLeft + w3*0 + w3/2, barY + barH/2 + 1);
    tft.drawString("|",  xLeft + w3*1,        barY + barH/2 + 1);
    tft.drawString(A1, xLeft + w3*1 + w3/2, barY + barH/2 + 1);
    tft.drawString("|",  xLeft + w3*2,        barY + barH/2 + 1);
    tft.drawString(A2, xLeft + w3*2 + w3/2, barY + barH/2 + 1);
  }
}

// ---------- Exemplo de “overlay de notificação” rápido ----------
// Preenche fundo suavemente e desenha o toast central.
static void showNotificationOverlay(TFT_eSPI &tft, const NotifyCard &n){
  const uint16_t bg = RGB(12,14,18);
  tft.fillScreen(bg);
  tft.drawCircle(120,120,118, RGB(40,45,55));
  drawNotifyToast(tft, n, 110, true);
}

#endif // NOTIFY_UI_H
