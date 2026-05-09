// esp32_image_receiver.h
// ESP32 + GC9A01 (240x240 round) + TFT_eSPI
// Recebe e exibe imagens do servidor Node.js

#ifndef ESP32_IMAGE_RECEIVER_H
#define ESP32_IMAGE_RECEIVER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// ---------- Configurações WiFi ----------
const char* ssid = "COMPANIA";
const char* password = "jesusateu123";
const char* serverURL = "http://192.168.0.100:3000"; // IP do seu computador (onde roda o servidor Node.js)

// ---------- Estrutura da imagem ----------
struct ReceivedImage {
  uint16_t* data = nullptr;
  int size = 0;
  String title = "";
  String date = "";
  bool loaded = false;
};

// ---------- Variáveis globais ----------
ReceivedImage currentImage;
TFT_eSPI* tftPtr = nullptr;

// ---------- Declarações de funções ----------
bool parseImageData(const String& fileContent);
void setupWebServer();
void handleReceiveImage();
void handleStatus();
void handleTest();
void handleForceUpdate();
void handleNotFound();
void handleServerRequests();
bool loadRandomImage();

// ---------- Funções auxiliares para conversão de cores ----------
uint8_t color565to8bitR(uint16_t color) {
  return (color >> 8) & 0xF8;
}

uint8_t color565to8bitG(uint16_t color) {
  return (color >> 3) & 0xFC;
}

uint8_t color565to8bitB(uint16_t color) {
  return (color << 3) & 0xF8;
}

// ---------- Função para conectar WiFi ----------
bool connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Conectado! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println();
    Serial.println("Falha na conexão WiFi");
    return false;
  }
}

// ---------- Função para receber imagem do servidor ----------
bool receiveImageFromServer(const String& filename) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não conectado");
    return false;
  }

  HTTPClient http;
  String url = String(serverURL) + "/esp32/image-data/" + filename;
  
  Serial.println("Baixando dados da imagem: " + url);
  http.begin(url);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse do JSON
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, payload);
    
    if (doc.containsKey("data") && doc.containsKey("size")) {
      // Limpa imagem anterior
      if (currentImage.data != nullptr) {
        free(currentImage.data);
        currentImage.data = nullptr;
      }
      
      // Aloca memória para os dados
      int size = doc["size"];
      currentImage.data = (uint16_t*)malloc(size * sizeof(uint16_t));
      if (currentImage.data == nullptr) {
        Serial.println("Erro ao alocar memória");
        http.end();
        return false;
      }
      
      currentImage.size = size;
      currentImage.loaded = true;
      
      // Copia os dados
      JsonArray dataArray = doc["data"];
      for (int i = 0; i < size && i < dataArray.size(); i++) {
        currentImage.data[i] = dataArray[i];
      }
      
      Serial.printf("Imagem recebida com sucesso! Tamanho: %d pixels\n", size);
      http.end();
      return true;
    } else {
      Serial.println("Formato JSON inválido");
    }
  } else {
    Serial.printf("Erro HTTP: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// ---------- Função para parsear dados da imagem ----------
bool parseImageData(const String& fileContent) {
  // Limpa imagem anterior
  if (currentImage.data != nullptr) {
    free(currentImage.data);
    currentImage.data = nullptr;
  }
  
  // Procura pelo array de dados
  int startIndex = fileContent.indexOf("const uint16_t imageData[] = {");
  if (startIndex == -1) {
    Serial.println("Formato de arquivo inválido");
    return false;
  }
  
  int dataStart = fileContent.indexOf("{", startIndex) + 1;
  int dataEnd = fileContent.indexOf("};", dataStart);
  
  if (dataStart == -1 || dataEnd == -1) {
    Serial.println("Dados da imagem não encontrados");
    return false;
  }
  
  String dataString = fileContent.substring(dataStart, dataEnd);
  dataString.trim();
  
  // Conta quantos valores existem
  int valueCount = 0;
  for (int i = 0; i < dataString.length(); i++) {
    if (dataString.charAt(i) == ',') {
      valueCount++;
    }
  }
  valueCount++; // Último valor não tem vírgula
  
  // Aloca memória para os dados
  currentImage.data = (uint16_t*)malloc(valueCount * sizeof(uint16_t));
  if (currentImage.data == nullptr) {
    Serial.println("Erro ao alocar memória");
    return false;
  }
  
  currentImage.size = valueCount;
  
  // Parse dos valores hexadecimais
  int dataIndex = 0;
  int lastIndex = 0;
  
  for (int i = 0; i <= dataString.length(); i++) {
    if (i == dataString.length() || dataString.charAt(i) == ',') {
      String valueStr = dataString.substring(lastIndex, i);
      valueStr.trim();
      
      // Remove "0x" se existir
      if (valueStr.startsWith("0x")) {
        valueStr = valueStr.substring(2);
      }
      
      // Converte para uint16_t
      currentImage.data[dataIndex] = (uint16_t)strtol(valueStr.c_str(), NULL, 16);
      dataIndex++;
      lastIndex = i + 1;
    }
  }
  
  currentImage.loaded = true;
  Serial.printf("Imagem carregada: %d pixels\n", currentImage.size);
  return true;
}

// ---------- Função para exibir imagem no display ----------
void displayImage() {
  if (!currentImage.loaded || currentImage.data == nullptr || tftPtr == nullptr) {
    Serial.println("Nenhuma imagem carregada para exibir");
    return;
  }
  
  TFT_eSPI& tft = *tftPtr;
  
  // Limpa a tela
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 110, TFT_WHITE);
  
  // Área da imagem (círculo interno)
  int imageR = 85;
  int cx = 120, cy = 120;
  
  // Verifica se temos dados suficientes (240x240 = 57600 pixels)
  int expectedSize = 240 * 240;
  if (currentImage.size < expectedSize) {
    Serial.printf("Tamanho da imagem incorreto: %d (esperado: %d)\n", currentImage.size, expectedSize);
    return;
  }
  
  // Usa sprite para renderização mais rápida
  TFT_eSprite sprite = TFT_eSprite(&tft);
  sprite.createSprite(240, 240);
  sprite.setColorDepth(16);
  
  // Desenha a imagem no sprite
  int pixelIndex = 0;
  for (int y = 0; y < 240 && pixelIndex < currentImage.size; y++) {
    for (int x = 0; x < 240 && pixelIndex < currentImage.size; x++) {
      sprite.drawPixel(x, y, currentImage.data[pixelIndex]);
      pixelIndex++;
    }
  }
  
  // Aplica máscara circular ao sprite
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      int dx = x - 120;
      int dy = y - 120;
      if (dx * dx + dy * dy > imageR * imageR) {
        sprite.drawPixel(x, y, TFT_BLACK);
      }
    }
  }
  
  // Desenha o sprite na tela
  sprite.pushSprite(0, 0);
  sprite.deleteSprite();
  
  // Informações da imagem
  int infoY = cy + imageR + 15;
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(currentImage.title, cx, infoY);
  
  tft.setTextFont(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(currentImage.date, cx, infoY + 15);
  
  Serial.println("Imagem exibida no display");
}

// ---------- Função alternativa para exibição mais rápida ----------
void displayImageFast() {
  if (!currentImage.loaded || currentImage.data == nullptr || tftPtr == nullptr) {
    Serial.println("Nenhuma imagem carregada para exibir");
    return;
  }
  
  TFT_eSPI& tft = *tftPtr;
  
  // Limpa a tela
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 110, TFT_WHITE);
  
  // Área da imagem (círculo interno)
  int imageR = 85;
  int cx = 120, cy = 120;
  
  // Verifica se temos dados suficientes
  int expectedSize = 240 * 240;
  if (currentImage.size < expectedSize) {
    Serial.printf("Tamanho da imagem incorreto: %d (esperado: %d)\n", currentImage.size, expectedSize);
    return;
  }
  
  // Desenha a imagem usando pushImage (mais rápido)
  tft.pushImage(0, 0, 240, 240, currentImage.data);
  
  // Aplica máscara circular
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      int dx = x - cx;
      int dy = y - cy;
      if (dx * dx + dy * dy > imageR * imageR) {
        tft.drawPixel(x, y, TFT_BLACK);
      }
    }
  }
  
  // Informações da imagem
  int infoY = cy + imageR + 15;
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(currentImage.title, cx, infoY);
  
  tft.setTextFont(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(currentImage.date, cx, infoY + 15);
  
  Serial.println("Imagem exibida no display (modo rápido)");
}

// ---------- Função para exibir imagem com animação ----------
void displayImageWithAnimation() {
  if (!currentImage.loaded || currentImage.data == nullptr || tftPtr == nullptr) {
    Serial.println("Nenhuma imagem carregada para exibir");
    return;
  }
  
  TFT_eSPI& tft = *tftPtr;
  
  // Limpa a tela
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 110, TFT_WHITE);
  
  // Área da imagem (círculo interno)
  int imageR = 85;
  int cx = 120, cy = 120;
  
  // Verifica se temos dados suficientes
  int expectedSize = 240 * 240;
  if (currentImage.size < expectedSize) {
    Serial.printf("Tamanho da imagem incorreto: %d (esperado: %d)\n", currentImage.size, expectedSize);
    return;
  }
  
  // Animação de fade-in
  for (int alpha = 0; alpha <= 100; alpha += 10) {
    // Desenha a imagem com transparência
    for (int y = 0; y < 240; y++) {
      for (int x = 0; x < 240; x++) {
        int dx = x - cx;
        int dy = y - cy;
        if (dx * dx + dy * dy <= imageR * imageR) {
          int pixelIndex = y * 240 + x;
          if (pixelIndex < currentImage.size) {
            uint16_t color = currentImage.data[pixelIndex];
            // Aplica transparência (simplificado)
            if (alpha < 100) {
              color = tft.color565(
                (color565to8bitR(color) * alpha) / 100,
                (color565to8bitG(color) * alpha) / 100,
                (color565to8bitB(color) * alpha) / 100
              );
            }
            tft.drawPixel(x, y, color);
          }
        }
      }
    }
    delay(50);
  }
  
  // Informações da imagem
  int infoY = cy + imageR + 15;
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(currentImage.title, cx, infoY);
  
  tft.setTextFont(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(currentImage.date, cx, infoY + 15);
  
  Serial.println("Imagem exibida no display (com animação)");
}

// ---------- Função para listar imagens disponíveis ----------
String* listAvailableImages(int& count) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não conectado");
    count = 0;
    return nullptr;
  }

  HTTPClient http;
  String url = String(serverURL) + "/images";
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);
    
    count = doc.size();
    String* images = new String[count];
    
    for (int i = 0; i < count; i++) {
      images[i] = doc[i]["filename"].as<String>();
    }
    
    http.end();
    return images;
  }
  
  http.end();
  count = 0;
  return nullptr;
}

// ---------- Servidor HTTP para receber imagens ----------
#include <WebServer.h>
WebServer server(80);

// ---------- Função para inicializar o sistema ----------
bool initializeImageSystem(TFT_eSPI& tft) {
  tftPtr = &tft;
  
  // Conecta WiFi
  if (!connectWiFi()) {
    return false;
  }
  
  // Configura servidor HTTP
  setupWebServer();
  server.begin();
  Serial.println("Servidor HTTP iniciado na porta 80");
  
  // Testa conexão com servidor
  HTTPClient http;
  String url = String(serverURL) + "/esp32/test";
  http.begin(url);
  int httpCode = http.GET();
  http.end();
  
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Servidor conectado com sucesso!");
    return true;
  } else {
    Serial.println("Erro ao conectar com servidor");
    return false;
  }
}

// ---------- Configuração do servidor web ----------
void setupWebServer() {
  // Rota para receber imagem do servidor Node.js
  server.on("/receive-image", HTTP_POST, handleReceiveImage);
  
  // Rota para status do ESP32
  server.on("/status", HTTP_GET, handleStatus);
  
  // Rota para testar conexão
  server.on("/test", HTTP_GET, handleTest);
  
  // Rota para forçar atualização
  server.on("/update", HTTP_POST, handleForceUpdate);
  
  // Rota 404
  server.onNotFound(handleNotFound);
}

// ---------- Handler para receber imagem ----------
void handleReceiveImage() {
  Serial.println("=== RECEBENDO IMAGEM VIA HTTP POST ===");
  
  if (server.hasArg("plain")) {
    String jsonData = server.arg("plain");
    Serial.println("Dados JSON recebidos:");
    Serial.println(jsonData.substring(0, 200) + "..."); // Primeiros 200 chars
    
    // Parse do JSON
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if (error) {
      Serial.printf("Erro ao parsear JSON: %s\n", error.c_str());
      server.send(400, "application/json", "{\"error\":\"Erro ao parsear JSON\"}");
      return;
    }
    
    Serial.println("JSON parseado com sucesso");
    
    if (doc.containsKey("data") && doc.containsKey("size")) {
      int size = doc["size"];
      Serial.printf("Tamanho da imagem: %d pixels\n", size);
      
      // Limpa imagem anterior
      if (currentImage.data != nullptr) {
        free(currentImage.data);
        currentImage.data = nullptr;
        Serial.println("Imagem anterior liberada");
      }
      
      // Aloca memória para os dados
      currentImage.data = (uint16_t*)malloc(size * sizeof(uint16_t));
      if (currentImage.data == nullptr) {
        Serial.println("ERRO: Falha ao alocar memória");
        server.send(500, "application/json", "{\"error\":\"Erro ao alocar memória\"}");
        return;
      }
      
      Serial.println("Memória alocada com sucesso");
      
      currentImage.size = size;
      currentImage.loaded = true;
      
      // Copia os dados
      JsonArray dataArray = doc["data"];
      Serial.printf("Copiando %d pixels...\n", dataArray.size());
      
      for (int i = 0; i < size && i < dataArray.size(); i++) {
        currentImage.data[i] = dataArray[i];
      }
      
      Serial.println("Dados copiados com sucesso");
      
      // Atualiza título e data se fornecidos
      if (doc.containsKey("title")) {
        currentImage.title = doc["title"].as<String>();
        Serial.println("Título atualizado: " + currentImage.title);
      }
      if (doc.containsKey("date")) {
        currentImage.date = doc["date"].as<String>();
        Serial.println("Data atualizada: " + currentImage.date);
      }
      
      Serial.printf("Imagem recebida via HTTP! Tamanho: %d pixels\n", size);
      
      // Exibe a imagem imediatamente
      if (tftPtr != nullptr) {
        Serial.println("Exibindo imagem no display...");
        displayImageFast();
        Serial.println("Imagem exibida com sucesso!");
      } else {
        Serial.println("ERRO: TFT pointer é nulo");
      }
      
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Imagem recebida e exibida\"}");
      Serial.println("Resposta enviada com sucesso");
    } else {
      Serial.println("ERRO: JSON não contém 'data' ou 'size'");
      server.send(400, "application/json", "{\"error\":\"Formato JSON inválido\"}");
    }
  } else {
    Serial.println("ERRO: Nenhum dado recebido");
    server.send(400, "application/json", "{\"error\":\"Dados não fornecidos\"}");
  }
  
  Serial.println("=== FIM DO RECEBIMENTO ===");
}

// ---------- Handler para status ----------
void handleStatus() {
  DynamicJsonDocument doc(512);
  doc["status"] = "online";
  doc["wifi"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
  doc["ip"] = WiFi.localIP().toString();
  doc["image_loaded"] = currentImage.loaded;
  doc["image_size"] = currentImage.size;
  doc["uptime"] = millis() / 1000;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ---------- Handler para teste ----------
void handleTest() {
  server.send(200, "application/json", "{\"connected\":true,\"message\":\"ESP32 funcionando\"}");
}

// ---------- Handler para forçar atualização ----------
void handleForceUpdate() {
  Serial.println("Atualização forçada solicitada");
  
  if (loadRandomImage()) {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Imagem atualizada\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Falha ao carregar nova imagem\"}");
  }
}

// ---------- Handler para 404 ----------
void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Rota não encontrada\"}");
}

// ---------- Função para processar requisições do servidor ----------
void handleServerRequests() {
  server.handleClient();
}

// ---------- Função para carregar e exibir imagem por nome ----------
bool loadAndDisplayImage(const String& filename) {
  if (receiveImageFromServer(filename)) {
    displayImage();
    return true;
  }
  return false;
}

// ---------- Função para carregar imagem aleatória ----------
bool loadRandomImage() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não conectado");
    return false;
  }

  HTTPClient http;
  String url = String(serverURL) + "/esp32/random-image";
  
  Serial.println("Obtendo imagem aleatória: " + url);
  http.begin(url);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse do JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    if (doc.containsKey("filename")) {
      String filename = doc["filename"];
      Serial.println("Imagem selecionada: " + filename);
      
      http.end();
      return loadAndDisplayImage(filename);
    } else {
      Serial.println("Formato JSON inválido");
    }
  } else {
    Serial.printf("Erro HTTP: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// ---------- Função para mostrar status de conexão ----------
void showConnectionStatus(TFT_eSPI& tft) {
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 110, TFT_WHITE);
  
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("ESP32 Image", 120, 100);
  tft.drawString("Receiver", 120, 120);
  
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("WiFi: OK", 120, 140);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Aguardando", 120, 160);
    tft.drawString("imagens...", 120, 175);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("WiFi: ERRO", 120, 140);
  }
}

#endif // ESP32_IMAGE_RECEIVER_H
