#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
// FIRST_EDIT: inclui bibliotecas OLED
#include <Wire.h>
#include <U8g2lib.h>
// THIRD_EDIT: biblioteca base64 nativa do ESP32 já está incluída
//para ligar o microfone deve ser ligado o pino 35 no out e o gain/gnd no gnd e o vdd no 3.3v
// Definições para gravação de áudio
#define MIC_PIN 35                    // Entrada analógica do microfone MAX9814 (mudado de 34 para 35)
#define SAMPLE_RATE 8000              // Taxa de amostragem 8kHz
#define RECORD_SECONDS 3              // Duração da gravação em segundos
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_SECONDS)
#define AUDIO_CHUNK_SIZE 512          // Tamanho dos blocos para envio

// Definições para sistema de botões
#define R4 33   // Linha 4 (compartilhada pelos dois botões)
#define C1 25   // Coluna 1 (S16)
#define C2 26   // Coluna 2 (S15)

// Variáveis para controle dos botões
bool s15Pressionado = false;
bool s16Pressionado = false;
unsigned long lastButtonCheck = 0;
const unsigned long buttonCheckInterval = 50; // Verifica botões a cada 50ms

// Buffer para armazenar o áudio gravado
uint8_t audioBuffer[BUFFER_SIZE];

// Variáveis para controle de áudio
bool gravandoAudio = false;
bool enviandoAudio = false;
unsigned long inicioGravacao = 0;
int indiceBuffer = 0;

// ===== Variáveis para novo modo STREAMING =====
bool streamingAudio = false;            // indica se estamos transmitindo em tempo real
uint8_t streamChunkBuf[AUDIO_CHUNK_SIZE];
int streamPos = 0;                      // posição atual no chunk
unsigned long lastStreamSampleMicros = 0;
uint32_t streamChunkIdx = 0;            // índice do chunk

// Declaração antecipada do objeto WebSocket usado mais abaixo
extern WebSocketsClient webSocket;

// Envia chunk parcial ou completo no modo streaming
void enviarChunkStreaming() {
  if (streamPos == 0) return; // nada para enviar

  // Converte o buffer em String bruta (8-bit)
  String raw = "";
  for (int i = 0; i < streamPos; i++) {
    raw += (char)streamChunkBuf[i];
  }

  // Codifica em base64
  String encoded = encodeBase64(raw);

  // totalChunks = -1 indica stream (duração indefinida)
  String pacote = "audio|" + String(streamChunkIdx++) + "|-1|" + encoded;
  webSocket.sendTXT(pacote);

  // Reinicia posição
  streamPos = 0;
}

String ssid;
String password;
String emailLogin;
Preferences preferences;
WebServer server(80);

String randomId; // parte aleatória gerada apenas uma vez
const int ledPin = 2;  // Pino do LED onboard (geralmente D2)
const int tempoGiro = 100; // tempo de giro do motor em ms
unsigned long lastPing = 0;
const unsigned long pingInterval = 50000; // 50 segundos

WebSocketsClient webSocket;

DNSServer dnsServer;
const byte DNS_PORT = 53;
// SECOND_EDIT: cria objeto U8g2 para display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);

// Variáveis para controle do display
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 200; // Atualiza display a cada 200ms
String lastCommand = "";
String statusMessage = "";

// Inserir diretiva para ignorar função de LED
#if 0  // Desativado: funcionalidade de piscar LED
// Função para piscar o LED 3 vezes
void piscarLed(int vezes) {
  for (int i = 0; i < vezes; i++) {
    digitalWrite(ledPin, HIGH);
    delay(300);
    digitalWrite(ledPin, LOW);
    delay(300);
  }
}
#endif

// Gera string aleatória alfanumérica
String gerarRandom(int len) {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  String res;
  for (int i = 0; i < len; i++) {
    res += charset[random(sizeof(charset) - 1)];
  }
  return res;
}

// Função de codificação base64 manual (fallback)
String encodeBase64(const String& input) {
  const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String result;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];
  int in_len = input.length();
  const char* bytes_to_encode = input.c_str();

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; i < 4; i++)
        result += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; j < i + 1; j++)
      result += base64_chars[char_array_4[j]];

    while ((i++ < 3))
      result += '=';
  }

  return result;
}

// Função para desenhar barra de notificação no topo (128x10)
// Definição de bitmap 8x8 para ícone Wi-Fi
static const unsigned char wifi_icon_bits[] PROGMEM = {
  0x7E, 0x81, 0x3C, 0x42, 0x18, 0x24, 0x18, 0x00
};
void drawNotificationBar() {
  // Fonte pequena para texto
  u8g2.setFont(u8g2_font_5x7_tf);

  // Indicador de áudio/gravação/streaming (texto curto) no canto esquerdo
  u8g2.setCursor(0, 8);
  if (streamingAudio) {
    u8g2.print("LIVE");
  } else if (gravandoAudio) {
    u8g2.print("REC");
  } else if (enviandoAudio) {
    u8g2.print("SEND");
  }

  // Desenha ícone Wi-Fi no centro (8x8)
  u8g2.drawXBM(60, 1, 8, 8, wifi_icon_bits);
  if (WiFi.status() != WL_CONNECTED) {
    // Traço diagonal indicando desconexão
    u8g2.drawLine(60, 1, 68, 9);
  }

  // ID no canto direito
  int idWidth = u8g2.getUTF8Width(randomId.c_str());
  u8g2.setCursor(128 - idWidth, 8);
  u8g2.print(randomId);

  // Linha de separação
  //u8g2.drawHLine(0, 10, 128);
}

// Função para verificar os botões
void verificarBotoes() {
  // Verifica S16 (C1 + R4) - Encerrar áudio
  digitalWrite(C1, LOW);
  digitalWrite(C2, HIGH);
  delay(1); // pequena espera
  if (digitalRead(R4) == LOW) {
    if (!s16Pressionado) {
      s16Pressionado = true;
      Serial.println("S16 pressionado - Encerrando audio");
      
      // Encerra o streaming de áudio
      if (streamingAudio) {
        streamingAudio = false;
        enviarChunkStreaming(); // Envia chunk final
        webSocket.sendTXT("audio|fim");
        mostrarMensagemStatus("Audio encerrado");
      }
      
      // Encerra gravação se estiver gravando
      if (gravandoAudio) {
        gravandoAudio = false;
        mostrarMensagemStatus("Gravacao encerrada");
      }
    }
  } else {
    s16Pressionado = false;
  }

  // Verifica S15 (C2 + R4) - Iniciar áudio
  digitalWrite(C1, HIGH);
  digitalWrite(C2, LOW);
  delay(1);
  if (digitalRead(R4) == LOW) {
    if (!s15Pressionado) {
      s15Pressionado = true;
      Serial.println("S15 pressionado - Iniciando audio");
      
      // Inicia/Encerra o streaming de áudio (toggle)
      if (streamingAudio) {
        streamingAudio = false;
        enviarChunkStreaming();
        webSocket.sendTXT("audio|fim");
        mostrarMensagemStatus("Audio encerrado");
      } else if (gravandoAudio) {
        // Estava apenas gravando local → encerra gravação
        gravandoAudio = false;
        mostrarMensagemStatus("Gravacao encerrada");
      } else {
        // Não estava ativo → inicia streaming
        streamingAudio = true;
        streamPos = 0;
        streamChunkIdx = 0;
        lastStreamSampleMicros = micros();
        mostrarMensagemStatus("Audio iniciado");
        webSocket.sendTXT("streaming_on");
      }
    }
  } else {
    s15Pressionado = false;
  }

  // Restaura colunas
  digitalWrite(C1, HIGH);
  digitalWrite(C2, HIGH);
}

// Funções melhoradas para o display OLED
void drawWrappedText(const char* text, uint8_t x, uint8_t y, uint8_t maxWidth) {
  u8g2.setFont(u8g2_font_5x7_tf);
  int lineHeight = 9;
  int cursorX = x;
  int cursorY = y;

  for (int i = 0; text[i] != '\0'; i++) {
    char c = text[i];
    if (cursorX + u8g2.getUTF8Width(&c) > maxWidth || c == '\n') {
      cursorX = x;
      cursorY += lineHeight;
      if (c == '\n') continue;
    }
    u8g2.setCursor(cursorX, cursorY);
    u8g2.print(c);
    cursorX += u8g2.getUTF8Width(&c);
  }
}

void mostrarStatusInicial() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título em fonte média
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("ESP32 Display");
  
  // Subtítulo
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 30);
  u8g2.print("Iniciando sistema...");
  u8g2.setCursor(0, 40);
  u8g2.print("Aguarde...");
  
  u8g2.sendBuffer();
}

void mostrarStatusWiFi() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("Status WiFi");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.setCursor(0, 30);
    u8g2.print("Conectado ao WiFi");
    u8g2.setCursor(0, 40);
    u8g2.print("IP: ");
    u8g2.print(WiFi.localIP().toString());
    u8g2.setCursor(0, 50);
    u8g2.print("RSSI: ");
    u8g2.print(WiFi.RSSI());
    u8g2.print(" dBm");
    u8g2.setCursor(0, 60);
    u8g2.print("SSID: ");
    u8g2.print(ssid);
  } else {
    u8g2.setCursor(0, 30);
    u8g2.print("○ Desconectado");
    u8g2.setCursor(0, 40);
    u8g2.print("Portal ativo");
    u8g2.setCursor(0, 50);
    u8g2.print("192.168.10.1");
    u8g2.setCursor(0, 60);
    u8g2.print("Rede: Porta automatica");
  }
  u8g2.sendBuffer();
}

void mostrarComando(const String& comando) {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("Comando:");
  
  // Texto do comando com quebra automática
  String texto = "> " + comando;
  drawWrappedText(texto.c_str(), 0, 30, 128);
  
  // Timestamp
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 70);
  u8g2.print("Recebido agora");
  
  u8g2.sendBuffer();
  lastCommand = comando;
}

void mostrarTexto(const String& texto) {
  mostrarComando(texto);
}
void mostrarMensagemStatus(const String& mensagem) {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Configura fonte e calcula número de caracteres por linha
  u8g2.setFont(u8g2_font_5x7_tf);
  const int charsPerLine = 26;
  
  // Quebra o texto em linhas
  String texto = mensagem;
  int startPos = 0;
  int yPos = 30;
  
  while (startPos < texto.length() && yPos < 64) {
    // Calcula quantos caracteres cabem na linha
    int endPos = min(startPos + charsPerLine, (int)texto.length());
    
    // Se não for o fim do texto, procura último espaço
    if (endPos < texto.length()) {
      int lastSpace = texto.lastIndexOf(' ', endPos);
      if (lastSpace > startPos) {
        endPos = lastSpace;
      }
    }
    
    // Extrai e exibe a linha
    String linha = texto.substring(startPos, endPos);
    u8g2.setCursor(0, yPos);
    u8g2.print(linha);
    
    startPos = endPos + 1;
    yPos += 10; // Espaçamento entre linhas
  }
  
  u8g2.sendBuffer();
  statusMessage = mensagem;
}

void mostrarInfoSistema() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("Sistema");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 30);
  u8g2.print("ID: ");
  u8g2.print(randomId);
  
  u8g2.setCursor(0, 40);
  u8g2.print("Uptime: ");
  u8g2.print(millis() / 1000);
  u8g2.print("s");
  
  u8g2.setCursor(0, 50);
  u8g2.print("Mem: ");
  u8g2.print(ESP.getFreeHeap());
  u8g2.print(" bytes");
  
  u8g2.setCursor(0, 60);
  u8g2.print("Email: ");
  u8g2.print(emailLogin);
  
  u8g2.sendBuffer();
}

// Função para mostrar informações detalhadas do sistema incluindo áudio
void mostrarInfoSistemaCompleto() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("Sistema Completo");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 30);
  u8g2.print("ID: ");
  u8g2.print(randomId);
  
  u8g2.setCursor(0, 40);
  u8g2.print("Audio: ");
  u8g2.print(SAMPLE_RATE);
  u8g2.print("Hz ");
  u8g2.print(RECORD_SECONDS);
  u8g2.print("s");
  
  u8g2.setCursor(0, 50);
  u8g2.print("Buffer: ");
  u8g2.print(BUFFER_SIZE);
  u8g2.print(" bytes");
  
  u8g2.setCursor(0, 60);
  u8g2.print("Mic: GPIO");
  u8g2.print(MIC_PIN);
  
  u8g2.sendBuffer();
}

// Função para mostrar status dos botões
void mostrarStatusBotoes() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("Botoes");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 30);
  u8g2.print("S15: Iniciar Audio");
  u8g2.setCursor(0, 40);
  u8g2.print("S16: Encerrar Audio");
  
  u8g2.setCursor(0, 50);
  if (streamingAudio) {
    u8g2.print("Status: Gravando");
  } else {
    u8g2.print("Status: Parado");
  }
  
  u8g2.setCursor(0, 60);
  u8g2.print("Mic: GPIO");
  u8g2.print(MIC_PIN);
  
  u8g2.sendBuffer();
}

// Função para gravar áudio do microfone
void gravarAudio() {
  mostrarMensagemStatus("Gravando audio...");
  gravandoAudio = true;
  indiceBuffer = 0;
  inicioGravacao = millis();
  
  // Grava por RECORD_SECONDS segundos
  while (indiceBuffer < BUFFER_SIZE) {
    int leitura = analogRead(MIC_PIN); // 0 a 4095
    audioBuffer[indiceBuffer] = leitura >> 4; // Converte para 8 bits (0 a 255)
    indiceBuffer++;
    delayMicroseconds(1000000 / SAMPLE_RATE); // Mantém taxa de amostragem
  }
  
  gravandoAudio = false;
  mostrarMensagemStatus("Audio gravado!");
}

// Função para enviar áudio via WebSocket em chunks
void enviarAudioWebSocket() {
  mostrarMensagemStatus("Enviando audio...");
  enviandoAudio = true;
  
  // Calcula quantos chunks serão enviados
  int totalChunks = (BUFFER_SIZE + AUDIO_CHUNK_SIZE - 1) / AUDIO_CHUNK_SIZE;
  
  for (int i = 0; i < totalChunks; i++) {
    int inicio = i * AUDIO_CHUNK_SIZE;
    int fim = min(inicio + AUDIO_CHUNK_SIZE, BUFFER_SIZE);
    int tamanhoChunk = fim - inicio;
    
    // Cria string com os dados do chunk
    String chunk = "";
    for (int j = inicio; j < fim; j++) {
      chunk += (char)audioBuffer[j];
    }
    
    // Codifica em base64 usando função manual
    String encoded = encodeBase64(chunk);
    
    // Envia com prefixo para identificação
    String pacote = "audio|" + String(i) + "|" + String(totalChunks) + "|" + encoded;
    webSocket.sendTXT(pacote);
    
    // Pequena pausa para não sobrecarregar
    delay(50);
  }
  
  // Sinaliza fim do envio
  webSocket.sendTXT("audio|fim");
  enviandoAudio = false;
  mostrarMensagemStatus("Audio enviado!");
}

// Função para mostrar status de gravação no display
void mostrarStatusAudio() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("Audio");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  if (gravandoAudio) {
    u8g2.setCursor(0, 30);
    u8g2.print("Gravando...");
    u8g2.setCursor(0, 40);
    u8g2.print("Tempo: ");
    u8g2.print((millis() - inicioGravacao) / 1000);
    u8g2.print("/");
    u8g2.print(RECORD_SECONDS);
    u8g2.print("s");
    u8g2.setCursor(0, 50);
    u8g2.print("Amostras: ");
    u8g2.print(indiceBuffer);
    u8g2.print("/");
    u8g2.print(BUFFER_SIZE);
  } else if (enviandoAudio) {
    u8g2.setCursor(0, 30);
    u8g2.print("Enviando...");
    u8g2.setCursor(0, 40);
    u8g2.print("Buffer: ");
    u8g2.print(BUFFER_SIZE);
    u8g2.print(" bytes");
    u8g2.setCursor(0, 50);
    u8g2.print("Chunks: ");
    u8g2.print((BUFFER_SIZE + AUDIO_CHUNK_SIZE - 1) / AUDIO_CHUNK_SIZE);
  } else {
    u8g2.setCursor(0, 30);
    u8g2.print("Pronto para gravar");
    u8g2.setCursor(0, 40);
    u8g2.print("Comando: audio");
    u8g2.setCursor(0, 50);
    u8g2.print("Duracao: ");
    u8g2.print(RECORD_SECONDS);
    u8g2.print("s");
  }
  
  u8g2.sendBuffer();
}

// Função para reproduzir áudio localmente via DAC (opcional)
void reproduzirAudioLocal() {
  mostrarMensagemStatus("Reproduzindo...");
  
  // Verifica se há dados no buffer
  if (indiceBuffer == 0) {
    mostrarMensagemStatus("Nenhum audio gravado");
    return;
  }
  
  // Reproduz o áudio gravado
  for (int i = 0; i < indiceBuffer; i++) {
    // Se tiver DAC conectado no GPIO25, descomente a linha abaixo:
    // dacWrite(25, audioBuffer[i]);
    
    // Por enquanto, apenas simula a reprodução
    delayMicroseconds(1000000 / SAMPLE_RATE);
  }
  
  mostrarMensagemStatus("Reproducao finalizada");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Desconectado do servidor WebSocket");
      mostrarMensagemStatus("WebSocket desconectado");
      break;
    case WStype_CONNECTED:
      Serial.println("Conectado ao servidor WebSocket");
      mostrarMensagemStatus("WebSocket conectado");
      {
        String identificador = ssid + "|" + password + "|" + emailLogin + "|" + randomId;
        webSocket.sendTXT(identificador);
      }
      break;
    case WStype_TEXT:
      Serial.printf("Comando recebido: %s\n", payload);

      String cmd = String((char*)payload);

      // Exibe comando no display
      mostrarComando(cmd);
      
      if (cmd == "limpar") {
        mostrarMensagemStatus("Limpando credenciais...");
        webSocket.sendTXT("Credenciais Wi-Fi apagadas. Reiniciando...");
        preferences.remove("ssid");
        preferences.remove("password");
        preferences.remove("email");
        preferences.end();
        WiFi.disconnect(true, true);
        delay(1000);
        ESP.restart();
      } else if (cmd == "status") {
        mostrarStatusWiFi();
        webSocket.sendTXT("Status exibido no display");
      } else if (cmd == "info") {
        mostrarInfoSistemaCompleto();
        webSocket.sendTXT("Informações do sistema exibidas");
      } else if (cmd == "audio") {
        // Grava e envia áudio
        gravarAudio();
        enviarAudioWebSocket();
        webSocket.sendTXT("Audio gravado e enviado em base64");
      } else if (cmd == "gravar") {
        // Apenas grava o áudio
        gravarAudio();
        webSocket.sendTXT("Audio gravado (use 'audio' para enviar)");
      } else if (cmd == "reproduzir") {
        reproduzirAudioLocal();
        webSocket.sendTXT("Audio reproduzido localmente");
      } else if (cmd == "stream_on") {
        streamingAudio = true;
        streamPos = 0;
        streamChunkIdx = 0;
        webSocket.sendTXT("streaming_on");
        mostrarMensagemStatus("Streaming ON");
      } else if (cmd == "stream_off") {
        streamingAudio = false;
        // Envia eventual resto do chunk
        enviarChunkStreaming();
        webSocket.sendTXT("audio|fim");
        mostrarMensagemStatus("Streaming OFF");
      } else if (cmd == "botoes") {
        mostrarStatusBotoes();
        webSocket.sendTXT("Status dos botões exibido");
      } else {
        // Comando não reconhecido
        mostrarMensagemStatus("Comando: " + cmd);
        webSocket.sendTXT("Comando recebido: " + cmd);
      }
      break;
  }
}

// ===== PORTAL DE CONFIGURAÇÃO =====
void handleRoot() {
  // Faz scan de redes Wi-Fi disponíveis
  int n = WiFi.scanNetworks();
  String options = "";
  for (int i = 0; i < n; i++) {
    String ssidItem = WiFi.SSID(i);
    if (ssidItem.length() == 0) continue; // ignora SSID oculto
    if (options.indexOf("value='" + ssidItem + "'") >= 0) continue; // evita duplicados
    int rssi = WiFi.RSSI(i);
    options += "<option value='" + ssidItem + "'>" + ssidItem + " (" + String(rssi) + " dBm)</option>";
  }
  if (options.length() == 0) {
    options = "<option>Nenhuma rede encontrada</option>";
  }

  String page = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset='utf-8'>
      <meta name='viewport' content='width=device-width,initial-scale=1'>
      <title>Configuração ESP32</title>
      <style>
        body{font-family:Arial,Helvetica,sans-serif;background:#FFFFFF;margin:0;padding:0;color:#11181C;}
        .container{max-width:400px;margin:40px auto;padding:24px;background:#F2F2F2;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,0.1);}        
        h2{text-align:center;color:#FF7A00;margin-bottom:24px;}
        label{display:block;margin-bottom:6px;font-weight:600;}
        select,input{width:100%;padding:10px;margin-bottom:16px;border:1px solid #E0E0E0;border-radius:6px;box-sizing:border-box;}
        button{width:100%;background:#FF7A00;color:#fff;padding:12px 0;border:none;border-radius:6px;font-size:16px;font-weight:bold;cursor:pointer;}
        button:active{opacity:0.9;}
        .refresh{background:#006AFF;margin-top:-8px;margin-bottom:16px;}
      </style>
      <script>
        function copySSID(){var sel=document.getElementById('ssidSelect');document.getElementById('ssid').value=sel.value;}
        function refresh(){location.reload();}
      </script>
    </head>
    <body>
      <div class='container'>
        <h2>Configurar Wi-Fi & Email</h2>
        <form action='/save' method='POST'>
          <label for='ssidSelect'>Rede Wi-Fi</label>
          <select id='ssidSelect' onchange='copySSID()'>
            %OPTIONS%
          </select>
          <input type='hidden' id='ssid' name='ssid'>
          <button type='button' class='refresh' onclick='refresh()'>Atualizar lista</button>
          <label for='password'>Senha</label>
          <input type='text' id='password' name='password' placeholder='Senha do Wi-Fi' required>
          <label for='email'>E-mail</label>
          <input type='email' id='email' name='email' placeholder='Seu e-mail' required>
          <button type='submit'>Salvar</button>
        </form>
      </div>
      <script>copySSID();</script>
    </body>
    </html>
  )rawliteral";

  page.replace("%OPTIONS%", options);
  server.send(200, "text/html", page);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("email")) {
    preferences.putString("ssid", server.arg("ssid"));
    preferences.putString("password", server.arg("password"));
    preferences.putString("email", server.arg("email"));
    server.send(200, "text/html", "<html><body><h2>Dados salvos! Reiniciando...</h2></body></html>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Parametros ausentes");
  }
}
void startConfigPortal() {
  IPAddress local_ip(192, 168, 10, 1);     // IP do AP
  IPAddress gateway(192, 168, 10, 1);      // Gateway padrão
  IPAddress subnet(255, 255, 255, 0);      // Máscara de sub-rede

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet); // Define IP personalizado
  WiFi.softAP("Porta automatica");              // Nome da rede Wi-Fi

  Serial.println("Portal de configuração ativo. Conecte-se à rede: Porta automatica");
  Serial.print("Acesse: http://");
  Serial.println(local_ip);

  dnsServer.start(DNS_PORT, "*", local_ip);     // Redireciona todo domínio para o IP do AP
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
// ===== FIM PORTAL DE CONFIGURAÇÃO =====

void setup() {
  pinMode(ledPin, OUTPUT);
  // Pinos de joystick removidos – controle será por giroscópio
  
  // Configuração do microfone
  pinMode(MIC_PIN, INPUT);
  
  // Configuração dos botões
  pinMode(C1, OUTPUT);
  pinMode(C2, OUTPUT);
  pinMode(R4, INPUT_PULLUP);
  digitalWrite(C1, HIGH);
  digitalWrite(C2, HIGH);
  
  Serial.begin(115200);
  randomSeed(esp_random());

  // Inicia barramento I2C nos pinos 21 (SDA) e 22 (SCL)
  Wire.begin(21, 22);

  // Inicializa display OLED
  u8g2.begin();
  mostrarStatusInicial();

  preferences.begin("credenciais", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  emailLogin = preferences.getString("email", "");

  // Carrega ou gera identificador aleatório único (5 caracteres)
  randomId = preferences.getString("rand", "");
  if (randomId.length() == 0) {
    randomId = gerarRandom(5);
    preferences.putString("rand", randomId);
  }

  if (ssid.length() == 0) {
    mostrarMensagemStatus("Portal de configuracao");
    startConfigPortal();
    return;
  }

  mostrarMensagemStatus("Conectando WiFi...");
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFalha ao conectar, iniciando portal de configuração.");
    mostrarMensagemStatus("Falha WiFi - Portal ativo");
    startConfigPortal();
    return;
  }

  Serial.println("\nWiFi conectado!");
  mostrarStatusWiFi();

  // Configurações do WebSocket
  webSocket.setReconnectInterval(5000);
  webSocket.setAuthorization("esp", "neurelix");
  webSocket.setExtraHeaders("Sec-WebSocket-Extensions:");
  webSocket.onEvent(webSocketEvent);
  webSocket.beginSSL("esp-conecta.neurelix.com.br", 443, "/");
}

void loop() {
  // Verifica os botões periodicamente
  if (millis() - lastButtonCheck > buttonCheckInterval) {
    verificarBotoes();
    lastButtonCheck = millis();
  }
  
  // Removido bloco de reinício do jogo Snake
  
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();

    // ===== Streaming em tempo real =====
    if (streamingAudio) {
      unsigned long nowMicros = micros();
      if (nowMicros - lastStreamSampleMicros >= (1000000UL / SAMPLE_RATE)) {
        lastStreamSampleMicros = nowMicros;

        int leitura = analogRead(MIC_PIN) >> 4; // 8 bits
        streamChunkBuf[streamPos++] = (uint8_t)leitura;

        if (streamPos >= AUDIO_CHUNK_SIZE) {
          enviarChunkStreaming();
        }
      }
    }

    if (millis() - lastPing > pingInterval) {
      webSocket.sendTXT("ping");
      Serial.println("ping");
      lastPing = millis();
    }
    
    // Atualiza display periodicamente
    if (millis() - lastDisplayUpdate > displayUpdateInterval) {
      if (gravandoAudio || enviandoAudio) {
        mostrarStatusAudio();
      } else if (streamingAudio) {
        mostrarStatusBotoes();
      } else if (lastCommand.length() == 0) {
        mostrarStatusWiFi();
      }
      lastDisplayUpdate = millis();
    }
    
    // Bloco de game over removido (Snake desativado)
  } else {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Atualiza display no portal de configuração
    if (millis() - lastDisplayUpdate > displayUpdateInterval) {
      mostrarMensagemStatus("Portal de configuracao");
      lastDisplayUpdate = millis();
    }
  }
}