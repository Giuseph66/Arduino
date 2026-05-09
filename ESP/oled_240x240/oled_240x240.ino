#include "User_Setup.h"
#include <TFT_eSPI.h>
#include "esp32_image_receiver.h"

TFT_eSPI tft;

void setup(){
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0); // Rotação normal para receber imagens
  
  // Inicializa o sistema de imagens
  if (initializeImageSystem(tft)) {
    Serial.println("Sistema inicializado com sucesso!");
    
    // Tenta carregar uma imagem aleatória
    if (loadRandomImage()) {
      Serial.println("Imagem carregada!");
    } else {
      showConnectionStatus(tft);
    }
  } else {
    Serial.println("Erro na inicialização");
    showConnectionStatus(tft);
  }
}

void loop(){
  static unsigned long lastCheck = 0;
  static unsigned long lastImageChange = 0;
  static int imageCounter = 0;
  
  // Processa requisições do servidor HTTP
  handleServerRequests();
  
  // Verifica conexão WiFi a cada 30 segundos
  if (millis() - lastCheck >= 30000) {
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado, tentando reconectar...");
      if (connectWiFi()) {
        Serial.println("WiFi reconectado!");
        Serial.print("IP do ESP32: ");
        Serial.println(WiFi.localIP());
      } else {
        showConnectionStatus(tft);
      }
    }
  }
  
  // Troca de imagem a cada 15 segundos (mais tempo para permitir envio manual)
  if (millis() - lastImageChange >= 15000) {
    lastImageChange = millis();
    imageCounter++;
    
    Serial.println("Tentando carregar nova imagem...");
    
    if (loadRandomImage()) {
      Serial.println("Nova imagem carregada!");
      // Usa a função de exibição rápida
      displayImageFast();
    } else {
      Serial.println("Falha ao carregar nova imagem");
      // Mostra status de erro
      tft.fillScreen(TFT_BLACK);
      tft.drawCircle(120, 120, 110, TFT_RED);
      tft.setTextDatum(MC_DATUM);
      tft.setTextFont(2);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("Erro ao", 120, 110);
      tft.drawString("carregar", 120, 130);
      tft.drawString("imagem", 120, 150);
    }
  }
  
  delay(100); // Delay menor para melhor responsividade do servidor HTTP
}
