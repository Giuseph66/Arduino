// ImageViewer.h
// ESP32 + GC9A01 (240x240 round) + TFT_eSPI
// Visualizador de imagens para tela redonda

#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include <TFT_eSPI.h>
#include <math.h>

// ---------- Util ----------
static inline uint16_t RGB(uint8_t r,uint8_t g,uint8_t b){
  return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
}

// ---------- Estrutura da imagem ----------
struct ImageData {
  const char* title = "Imagem";
  const char* date = "Hoje";
  uint16_t bgColor = RGB(20, 20, 30);
  bool isPortrait = true;  // true = retrato, false = paisagem
  int imageType = 0;       // 0 = casal, 1 = paisagem, 2 = retrato
};

// ---------- Desenha imagem de casal (estilo das fotos enviadas) ----------
static void drawCoupleImage(TFT_eSPI &tft, int cx, int cy, int radius) {
  // Fundo da imagem
  tft.fillCircle(cx, cy, radius, RGB(240, 240, 250));
  
  // Desenha o casal em estilo minimalista
  int headSize = 8;
  int bodyWidth = 12;
  int bodyHeight = 16;
  
  // Mulher (direita)
  int womanX = cx + 15;
  int womanY = cy - 5;
  
  // Cabelo da mulher (volume)
  tft.fillCircle(womanX, womanY - headSize - 2, headSize + 3, RGB(40, 40, 40));
  tft.fillCircle(womanX, womanY - headSize - 1, headSize + 2, RGB(60, 60, 60));
  
  // Cabeça da mulher
  tft.fillCircle(womanX, womanY, headSize, RGB(255, 220, 200));
  tft.drawCircle(womanX, womanY, headSize, RGB(200, 150, 120));
  
  // Corpo da mulher (vestido)
  tft.fillRect(womanX - bodyWidth/2, womanY + headSize, bodyWidth, bodyHeight, RGB(200, 150, 200));
  tft.drawRect(womanX - bodyWidth/2, womanY + headSize, bodyWidth, bodyHeight, RGB(150, 100, 150));
  
  // Homem (esquerda)
  int manX = cx - 15;
  int manY = cy - 5;
  
  // Cabelo do homem
  tft.fillRect(manX - headSize, manY - headSize - 2, headSize * 2, headSize + 2, RGB(40, 40, 40));
  
  // Cabeça do homem
  tft.fillCircle(manX, manY, headSize, RGB(255, 200, 180));
  tft.drawCircle(manX, manY, headSize, RGB(200, 150, 120));
  
  // Barba do homem
  tft.fillRect(manX - 6, manY + 4, 12, 8, RGB(60, 60, 60));
  tft.drawRect(manX - 6, manY + 4, 12, 8, RGB(40, 40, 40));
  
  // Corpo do homem (camisa)
  tft.fillRect(manX - bodyWidth/2, manY + headSize, bodyWidth, bodyHeight, RGB(100, 150, 200));
  tft.drawRect(manX - bodyWidth/2, manY + headSize, bodyWidth, bodyHeight, RGB(80, 120, 180));
  
  // Braços se abraçando
  tft.drawLine(manX + 6, manY + headSize + 4, womanX - 6, womanY + headSize + 4, RGB(255, 200, 180));
  tft.drawLine(manX + 6, manY + headSize + 8, womanX - 6, womanY + headSize + 8, RGB(255, 200, 180));
  
  // Corações flutuando
  for (int i = 0; i < 3; i++) {
    int heartX = cx + random(-30, 30);
    int heartY = cy - 25 + i * 8;
    tft.fillCircle(heartX, heartY, 2, RGB(255, 100, 100));
    tft.fillCircle(heartX - 2, heartY, 2, RGB(255, 100, 100));
    tft.fillCircle(heartX + 2, heartY, 2, RGB(255, 100, 100));
  }
}

// ---------- Desenha paisagem ----------
static void drawLandscapeImage(TFT_eSPI &tft, int cx, int cy, int radius) {
  // Fundo gradiente (céu)
  for (int y = cy - radius; y < cy; y++) {
    int blue = 200 - (cy - y) * 2;
    if (blue < 100) blue = 100;
    tft.drawLine(cx - radius, y, cx + radius, y, RGB(100, 150, blue));
  }
  
  // Montanhas
  int mountainPoints[] = {
    cx - radius, cy + 20,
    cx - 40, cy - 30,
    cx - 10, cy - 10,
    cx + 20, cy - 40,
    cx + 50, cy - 20,
    cx + radius, cy + 10
  };
  
  for (int i = 0; i < 5; i += 2) {
    tft.fillTriangle(mountainPoints[i], mountainPoints[i+1], 
                     mountainPoints[i+2], mountainPoints[i+3],
                     cx, cy + 30, RGB(80, 120, 80));
  }
  
  // Sol
  tft.fillCircle(cx + 30, cy - 30, 15, RGB(255, 255, 100));
  tft.drawCircle(cx + 30, cy - 30, 15, RGB(255, 200, 0));
  
  // Nuvens
  for (int i = 0; i < 3; i++) {
    int cloudX = cx - 20 + i * 20;
    int cloudY = cy - 40;
    tft.fillCircle(cloudX, cloudY, 8, RGB(240, 240, 240));
    tft.fillCircle(cloudX + 8, cloudY, 6, RGB(240, 240, 240));
    tft.fillCircle(cloudX - 8, cloudY, 6, RGB(240, 240, 240));
  }
}

// ---------- Desenha retrato ----------
static void drawPortraitImage(TFT_eSPI &tft, int cx, int cy, int radius) {
  // Fundo da imagem
  tft.fillCircle(cx, cy, radius, RGB(200, 180, 160));
  
  // Pessoa centralizada
  int headSize = 12;
  int bodyWidth = 16;
  int bodyHeight = 20;
  
  // Cabelo
  tft.fillCircle(cx, cy - headSize - 3, headSize + 4, RGB(60, 40, 20));
  tft.fillCircle(cx, cy - headSize - 2, headSize + 3, RGB(80, 60, 30));
  
  // Cabeça
  tft.fillCircle(cx, cy, headSize, RGB(255, 220, 200));
  tft.drawCircle(cx, cy, headSize, RGB(200, 150, 120));
  
  // Olhos
  tft.fillCircle(cx - 4, cy - 2, 2, RGB(40, 40, 40));
  tft.fillCircle(cx + 4, cy - 2, 2, RGB(40, 40, 40));
  tft.fillCircle(cx - 4, cy - 2, 1, RGB(255, 255, 255));
  tft.fillCircle(cx + 4, cy - 2, 1, RGB(255, 255, 255));
  
  // Nariz
  tft.drawLine(cx, cy, cx, cy + 2, RGB(200, 150, 120));
  
  // Boca
  tft.drawLine(cx - 3, cy + 4, cx + 3, cy + 4, RGB(200, 100, 100));
  
  // Corpo (camisa)
  tft.fillRect(cx - bodyWidth/2, cy + headSize, bodyWidth, bodyHeight, RGB(100, 150, 200));
  tft.drawRect(cx - bodyWidth/2, cy + headSize, bodyWidth, bodyHeight, RGB(80, 120, 180));
  
  // Braços
  tft.fillRect(cx - bodyWidth/2 - 4, cy + headSize + 4, 4, 12, RGB(255, 220, 200));
  tft.fillRect(cx + bodyWidth/2, cy + headSize + 4, 4, 12, RGB(255, 220, 200));
}

// ---------- Função principal do visualizador ----------
static void drawImageViewer(TFT_eSPI &tft, const ImageData &img) {
  const int CX = 120, CY = 120;
  const int R = 110;
  
  // Fundo
  tft.fillScreen(RGB(12, 14, 18));
  tft.drawCircle(CX, CY, R, RGB(40, 45, 55));
  
  // Área da imagem (círculo interno)
  int imageR = 85;
  tft.fillCircle(CX, CY, imageR, img.bgColor);
  tft.drawCircle(CX, CY, imageR, RGB(60, 70, 85));
  
  // Desenha a imagem baseada no tipo
  switch (img.imageType) {
    case 0: // Casal
      drawCoupleImage(tft, CX, CY, imageR - 5);
      break;
    case 1: // Paisagem
      drawLandscapeImage(tft, CX, CY, imageR - 5);
      break;
    case 2: // Retrato
      drawPortraitImage(tft, CX, CY, imageR - 5);
      break;
  }
  
  // Informações da imagem (parte inferior)
  int infoY = CY + imageR + 15;
  
  // Título
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(RGB(255, 255, 255), RGB(12, 14, 18));
  tft.drawString(img.title, CX, infoY);
  
  // Data
  tft.setTextFont(1);
  tft.setTextColor(RGB(150, 160, 180), RGB(12, 14, 18));
  tft.drawString(img.date, CX, infoY + 15);
  
  // Indicadores de navegação
  tft.fillCircle(CX - 40, infoY + 25, 3, RGB(100, 100, 100));
  tft.fillCircle(CX, infoY + 25, 3, RGB(200, 200, 200));
  tft.fillCircle(CX + 40, infoY + 25, 3, RGB(100, 100, 100));
}

// ---------- Função para trocar de imagem ----------
static void nextImage(ImageData &img) {
  img.imageType = (img.imageType + 1) % 3;
  
  switch (img.imageType) {
    case 0:
      img.title = "Casal Feliz";
      img.date = "15/01/2025";
      img.bgColor = RGB(240, 240, 250);
      break;
    case 1:
      img.title = "Paisagem";
      img.date = "10/01/2025";
      img.bgColor = RGB(100, 150, 200);
      break;
    case 2:
      img.title = "Retrato";
      img.date = "05/01/2025";
      img.bgColor = RGB(200, 180, 160);
      break;
  }
}

#endif // IMAGE_VIEWER_H
