#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
// FIRST_EDIT: inclui bibliotecas OLED
#include <Wire.h>
#include <U8g2lib.h>
// THIRD_EDIT: biblioteca base64 nativa do ESP32 já está incluída
// ICONS: inclui arquivo de ícones
#include "icons.h"
// KEYPAD: inclui biblioteca para matriz de botões
#include <Keypad.h>
// KEYPAD_CONFIG: inclui configurações do teclado
#include "keypad_config.h"
// TIME: inclui biblioteca para sincronização de tempo
#include <time.h>
// MPU6050: inclui bibliotecas do acelerômetro/giroscópio
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
//para ligar o microfone deve ser ligado o pino 35 no out e o gain/gnd no gnd e o vdd no 3.3v
// Definições para gravação de áudio
#define MIC_PIN 35                    // Entrada analógica do microfone MAX9814 (mudado de 34 para 35)
#define SAMPLE_RATE 8000              // Taxa de amostragem 8kHz
#define RECORD_SECONDS 3              // Duração da gravação em segundos
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_SECONDS)
#define AUDIO_CHUNK_SIZE 512          // Tamanho dos blocos para envio

// Definições para sistema de botões - Matriz 4x4 completa
const byte LINHAS = KEYPAD_ROWS;
const byte COLUNAS = KEYPAD_COLS;

char teclas[LINHAS][COLUNAS] = KEYPAD_KEYS;

byte pinosLinhas[LINHAS] = ROW_PINS;
byte pinosColunas[COLUNAS] = COL_PINS;

Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// Nomes das teclas (S1–S16)
const char* nomesTeclas[16] = BUTTON_NAMES;

// Variáveis para detectar o tempo de pressionamento
char teclaAtual = NO_KEY;
unsigned long tempoInicio = 0;
bool pressionando = false;

// ===== CONFIGURAÇÃO NTP =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -4 * 3600; // GMT-4 (Brasil - horário padrão)
const int daylightOffset_sec = 0; // Sem horário de verão
bool ntpSincronizado = false;

// ===== CONFIGURAÇÃO MPU6050 =====
#define SDA_PIN 21  // Usa o mesmo pino do display OLED
#define SCL_PIN 22  // Usa o mesmo pino do display OLED
Adafruit_MPU6050 mpu;
bool mpuDisponivel = false;

// Variáveis para dados do MPU6050
float accelX = 0, accelY = 0, accelZ = 0;
float gyroX = 0, gyroY = 0, gyroZ = 0;
float tempMPU = 0;
unsigned long lastMPURead = 0;
const unsigned long mpuReadInterval = 100; // Lê MPU a cada 100ms

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

// Declaração antecipada das funções de áudio
uint8_t processarAmostraAudio(int leituraRaw);
void reiniciarCalibracaoDC();

// Declaração antecipada da função de ícones
void mostrarIconeCentro(const unsigned char* icon, int largura, int altura, const String& texto);
void mostrarRelogio();
void mostrarStatusGravacao();

void atualizarRelogio(bool force);
// Declaração antecipada do objeto WebSocket usado mais abaixo
extern WebSocketsClient webSocket;

// Envia chunk parcial ou completo no modo streaming
void enviarChunkStreaming() {
  if (streamPos == 0) return; // nada para enviar

  // Debug: mostra informações do chunk sendo enviado
  Serial.printf("Enviando chunk #%d, tamanho: %d bytes\n", streamChunkIdx, streamPos);

  // Converte o buffer em String bruta (8-bit)
  String raw = "";
  for (int i = 0; i < streamPos; i++) {
    raw += (char)streamChunkBuf[i];
  }

  // Codifica em base64
  String encoded = encodeBase64(raw);

  // totalChunks = -1 indica stream (duração indefinida)
  String pacote = "audio|" + String(streamChunkIdx++) + "|-1|" + encoded;
  
  // Envia o pacote via WebSocket
  bool enviado = webSocket.sendTXT(pacote);
  
  if (enviado) {
    Serial.printf("Chunk #%d enviado com sucesso\n", streamChunkIdx - 1);
  } else {
    Serial.printf("ERRO: Falha ao enviar chunk #%d\n", streamChunkIdx - 1);
  }

  // Reinicia posição para próximo chunk
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
const unsigned long displayUpdateInterval = 200; // Atualiza display a cada 200ms (igual ao microfone.ino)
String lastCommand = "";
String statusMessage = "";
// Duração de exibição de comandos
unsigned long lastCommandMillis = 0;
const unsigned long commandDisplayDuration = 100; // 0.1s

// Retenção de visualização após ação de botão (5s)
#define RENDER_NONE 0
#define RENDER_CLOCK 1
#define RENDER_STATUS_WIFI 2
#define RENDER_INFO_SISTEMA 3
#define RENDER_INFO_COMPLETO 4
#define RENDER_STATUS_AUDIO 5
#define RENDER_STATUS_BOTOES 6
#define RENDER_COMANDO 7
#define RENDER_STATUS_MSG 8
#define RENDER_ICON 9
#define RENDER_MPU 10

uint8_t heldRender = RENDER_NONE;
unsigned long actionHoldUntil = 0;
// Dados para re-renderização de ícone
const unsigned char* heldIconPtr = nullptr;
int heldIconW = 0;
int heldIconH = 0;
String heldIconText = "";

static inline void setHold(uint8_t kind, unsigned long ms) {
  heldRender = kind;
  actionHoldUntil = millis() + ms;
}

void renderHeldView() {
  switch (heldRender) {
    case RENDER_STATUS_WIFI:      mostrarStatusWiFi(); break;
    case RENDER_INFO_SISTEMA:     mostrarInfoSistema(); break;
    case RENDER_INFO_COMPLETO:    mostrarInfoSistemaCompleto(); break;
    case RENDER_STATUS_AUDIO:     mostrarStatusAudio(); break;
    case RENDER_STATUS_BOTOES:    mostrarStatusBotoes(); break;
    case RENDER_COMANDO:          mostrarComando(lastCommand); break;
    case RENDER_STATUS_MSG:       mostrarMensagemStatus(statusMessage); break;
    case RENDER_ICON:             mostrarIconeCentro(heldIconPtr, heldIconW, heldIconH, heldIconText); break;
    case RENDER_MPU:              mostrarDadosMPU(); break;
    case RENDER_CLOCK:            mostrarRelogio(); break;
    default:                      mostrarRelogio(); break;
  }
}

// Variáveis para controle do relógio
unsigned long lastClockMillis = 0;
int lastClockMinute = -1; // Último minuto exibido
String lastClockString = ""; // Última string do relógio exibida

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

void drawNotificationBar() {
  // Fonte pequena para texto
  u8g2.setFont(u8g2_font_5x7_tf);

  // Ícone do microfone 8x8 quando estiver gravando (sem texto)
  if (gravandoAudio || streamingAudio) {
    // Posiciona o ícone 8x8 no lado esquerdo
    u8g2.drawXBM(2, 0, 8, 8, mic);
  }

  // Desenha ícone Wi-Fi no lado esquerdo (após o microfone se ativo)
  int wifiX = (gravandoAudio || streamingAudio) ? 12 : 2; // Ajusta posição baseado no microfone
  u8g2.drawXBM(wifiX, 0, 8, 8, wifi_icon_bits);
  if (WiFi.status() != WL_CONNECTED) {
    // Traço diagonal indicando desconexão
    u8g2.drawLine(wifiX, 1, wifiX + 8, 9);
  }

  // Relógio digital no centro (HH:MM:SS) - usa string otimizada
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Usa a string do relógio otimizada
  String horaFormatada = lastClockString.length() > 0 ? lastClockString : "00:00:00";
  
  // Centraliza o relógio
  int larguraHora = u8g2.getUTF8Width(horaFormatada.c_str());
  int posXRelogio = (128 - larguraHora) / 2;
  
  u8g2.setCursor(posXRelogio, 8);
  u8g2.print(horaFormatada);

  // ID no canto direito
  u8g2.setFont(u8g2_font_5x7_tf);
  int idWidth = u8g2.getUTF8Width(randomId.c_str());
  u8g2.setCursor(128 - idWidth, 8);
  u8g2.print(randomId);
}

// Função para verificar os botões da matriz 4x4
void verificarBotoes() {
  // Processa múltiplas teclas e seus estados com a API da Keypad
  if (!teclado.getKeys()) {
    return;
  }

  static unsigned long pressStart[16] = {0};
  static bool isPressed[16] = {false};

  for (int i = 0; i < LIST_MAX; i++) {
    if (teclado.key[i].kchar == NO_KEY) continue;

    char keyChar = teclado.key[i].kchar;
    KeyState keyState = teclado.key[i].kstate;
    int index = mapearTecla(keyChar);
    if (index == -1 || index >= 16) continue;

    if (keyState == PRESSED) {
      isPressed[index] = true;
      pressStart[index] = millis();

      Serial.print("Botão ");
      Serial.print(nomesTeclas[index]);
      Serial.println(" pressionado");
    } else if (keyState == RELEASED) {
      unsigned long duracao = 0;
      if (isPressed[index]) {
        duracao = millis() - pressStart[index];
        isPressed[index] = false;
      }

      Serial.print("Botão ");
      Serial.print(nomesTeclas[index]);
      Serial.printf(" → Duração: %lums", duracao);

      if (duracao >= LONG_PRESS_THRESHOLD) {
        Serial.println(" → Clique LONGO");
        executarAcaoBotao(index, true);
      } else {
        Serial.println(" → Clique normal");
        executarAcaoBotao(index, false);
      }
    }
  }
}

// Função auxiliar para mapear a tecla para o índice S1–S16
int mapearTecla(char tecla) {
  // Mapeia dinamicamente baseado na matriz definida em KEYPAD_KEYS
  for (int r = 0; r < LINHAS; r++) {
    for (int c = 0; c < COLUNAS; c++) {
      if (teclas[r][c] == tecla) {
        return r * COLUNAS + c; // índice 0..15 em ordem linha-major
      }
    }
  }
  return -1;
}

// Função para executar ações específicas para cada botão
void executarAcaoBotao(int index, bool longPress) {
  switch(index) {
    case 0: // S1
      if (longPress) {
        mostrarInfoSistema();
        setHold(RENDER_INFO_SISTEMA, 5000);
      } else {
        mostrarRelogio(); // Exibe relógio no centro
        setHold(RENDER_CLOCK, 5000);
      }
      break;
      
    case 1: // S2
      if (longPress) {
        mostrarInfoSistemaCompleto();
        setHold(RENDER_INFO_COMPLETO, 5000);
      } else {
        mostrarStatusAudio();
        setHold(RENDER_STATUS_AUDIO, 5000);
      }
      break;
      
    case 2: // S3
      if (longPress) {
        mostrarStatusBotoes();
        setHold(RENDER_STATUS_BOTOES, 5000);
      } else {
        mostrarMensagemStatus("Botão S3 - Clique curto");
        setHold(RENDER_STATUS_MSG, 5000);
      }
      break;
      
    case 3: // S4
      if (longPress) {
        mostrarMensagemStatus("Botão S4 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarMensagemStatus("Botão S4 - Clique curto");
        setHold(RENDER_STATUS_MSG, 5000);
      }
      break;
      
    case 4: // S5
      if (longPress) {
        mostrarMensagemStatus("Botão S5 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarMensagemStatus("Botão S5 - Clique curto");
        setHold(RENDER_STATUS_MSG, 5000);
      }
      break;
      
    case 5: // S6
      if (longPress) {
        mostrarMensagemStatus("Botão S6 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarMensagemStatus("Botão S6 - Clique curto");
        setHold(RENDER_STATUS_MSG, 5000);
      }
      break;
      
    case 6: // S7
      if (longPress) {
        mostrarMensagemStatus("Botão S7 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarMensagemStatus("Botão S7 - Clique curto");
        setHold(RENDER_STATUS_MSG, 5000);
      }
      break;
      
    case 7: // S8
      if (longPress) {
        mostrarMensagemStatus("Botão S8 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarMensagemStatus("Botão S8 - Clique curto");
        setHold(RENDER_STATUS_MSG, 5000);
      }
      break;
      
    case 8: // S9
      if (longPress) {
        mostrarMensagemStatus("Botão S9 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarMensagemStatus("Botão S9 - Clique curto");
        setHold(RENDER_STATUS_MSG, 5000);
      }
      break;
      
    case 9: // S10
      if (longPress) {
        mostrarMensagemStatus("Botão S10 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarDadosMPU();
        setHold(RENDER_MPU, 5000);
      }
      break;
      
    case 10: // S11
      if (longPress) {
        mostrarMensagemStatus("S11 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {  
        mostrarIconeCentro(coffe, 32, 32, "Cafe!");
        heldIconPtr = coffe; heldIconW = 32; heldIconH = 32; heldIconText = "Cafe!";
        setHold(RENDER_ICON, 5000);
      }
      break;
      
    case 11: // S12
      if (longPress) {
        mostrarMensagemStatus("Botão S12 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarIconeCentro(apple, 32, 32, "Maca!");
        heldIconPtr = apple; heldIconW = 32; heldIconH = 32; heldIconText = "Maca!";
        setHold(RENDER_ICON, 5000);
      }
      break;
      
    case 12: // S13
      if (longPress) {
        mostrarMensagemStatus("Botão S13 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarIconeCentro(android, 32, 32, "Android!");
        heldIconPtr = android; heldIconW = 32; heldIconH = 32; heldIconText = "Android!";
        setHold(RENDER_ICON, 5000);
      }
      break;
      
    case 13: // S14
      if (longPress) {
        mostrarMensagemStatus("Botão S14 - Clique longo");
        setHold(RENDER_STATUS_MSG, 5000);
      } else {
        mostrarIconeCentro(whatts, 32, 32, "Whatts");
        heldIconPtr = whatts; heldIconW = 32; heldIconH = 32; heldIconText = "Whatts";
        setHold(RENDER_ICON, 5000);
      }
      break;
      
    case 14: // S15 - Iniciar/Encerrar áudio (mantém funcionalidade original)
      if (longPress) {
        if (streamingAudio) {
          Serial.println("=== STREAMING ENCERRADO (S15 - Longo) ===");
          Serial.printf("Total de chunks enviados: %d\n", streamChunkIdx);
          Serial.printf("Total de amostras capturadas: %d\n", streamChunkIdx * AUDIO_CHUNK_SIZE);
          Serial.printf("Tempo total de streaming: %.2f segundos\n", 
                       (float)(streamChunkIdx * AUDIO_CHUNK_SIZE) / SAMPLE_RATE);
          Serial.println("=====================================");
          
          streamingAudio = false;
          enviarChunkStreaming();
          webSocket.sendTXT("audio|fim");
          mostrarMensagemStatus("Audio encerrado (longo)");
          setHold(RENDER_STATUS_MSG, 5000);
        } else {
          mostrarMensagemStatus("S15 - Clique longo");
          setHold(RENDER_STATUS_MSG, 5000);
        }
      } else {
        if (streamingAudio) {
          Serial.println("=== STREAMING ENCERRADO ===");
          Serial.printf("Total de chunks enviados: %d\n", streamChunkIdx);
          Serial.printf("Total de amostras capturadas: %d\n", streamChunkIdx * AUDIO_CHUNK_SIZE);
          Serial.printf("Tempo total de streaming: %.2f segundos\n", 
                       (float)(streamChunkIdx * AUDIO_CHUNK_SIZE) / SAMPLE_RATE);
          Serial.println("=========================");
          
          streamingAudio = false;
          enviarChunkStreaming(); // Envia chunk final se houver
          webSocket.sendTXT("audio|fim");
          mostrarMensagemStatus("Audio encerrado");
          setHold(RENDER_STATUS_MSG, 5000);
        } else if (gravandoAudio) {
          gravandoAudio = false;
          mostrarMensagemStatus("Gravacao encerrada");
          setHold(RENDER_STATUS_MSG, 5000);
        } else {
          streamingAudio = true;
          streamPos = 0;
          streamChunkIdx = 0;
          lastStreamSampleMicros = micros();
          
          Serial.println("=== STREAMING INICIADO ===");
          Serial.printf("Taxa de amostragem: %d Hz\n", SAMPLE_RATE);
          Serial.printf("Tamanho do chunk: %d bytes\n", AUDIO_CHUNK_SIZE);
          Serial.printf("Tempo entre chunks: %.2f ms\n", (float)AUDIO_CHUNK_SIZE / SAMPLE_RATE * 1000);
          Serial.printf("Chunks por segundo: %.2f\n", (float)SAMPLE_RATE / AUDIO_CHUNK_SIZE);
          Serial.printf("Taxa de dados: %.2f KB/s\n", (float)SAMPLE_RATE / 1024);
          Serial.println("========================");
          
          mostrarMensagemStatus("Audio iniciado");
          webSocket.sendTXT("streaming_on");
          setHold(RENDER_STATUS_MSG, 5000);
        }
      }
      break;
      
    case 15: // S16 - Encerrar áudio (mantém funcionalidade original)
      if (longPress) {
        if (streamingAudio || gravandoAudio) {
          mostrarMensagemStatus("Reiniciando...");
          ESP.restart();
        }
      } else {
        if (streamingAudio) {
          Serial.println("=== STREAMING ENCERRADO (S16) ===");
          Serial.printf("Total de chunks enviados: %d\n", streamChunkIdx);
          Serial.printf("Total de amostras capturadas: %d\n", streamChunkIdx * AUDIO_CHUNK_SIZE);
          Serial.printf("Tempo total de streaming: %.2f segundos\n", 
                       (float)(streamChunkIdx * AUDIO_CHUNK_SIZE) / SAMPLE_RATE);
          Serial.println("================================");
          
          streamingAudio = false;
          enviarChunkStreaming(); // Envia chunk final se houver
          webSocket.sendTXT("audio|fim");
          mostrarMensagemStatus("Audio encerrado");
          setHold(RENDER_STATUS_MSG, 5000);
        }
        
        if (gravandoAudio) {
          gravandoAudio = false;
          mostrarMensagemStatus("Gravacao encerrada");
          setHold(RENDER_STATUS_MSG, 5000);
        }
      }
      break;
  }
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
    u8g2.print("Rede: Relogio inteligente");
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
  lastCommandMillis = millis();
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

// Função genérica para exibir ícone no centro do display
void mostrarIconeCentro(const unsigned char* icon, int largura, int altura, const String& texto = "") {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Calcula posição central para o ícone
  // Display: 128x64
  int iconX = (128 - largura) / 2;
  int iconY = (64 - altura) / 2;
  
  // Desenha o ícone no centro
  u8g2.drawXBM(iconX, iconY, largura, altura, icon);
  
  // Adiciona texto abaixo do ícone (se fornecido)
  if (texto.length() > 0) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 56);
    u8g2.print(texto);
  }
  
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

// Função para exibir status de gravação com ícone do microfone
void mostrarStatusGravacao() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("Gravando Audio");
  
  // Ícone do microfone 8x8 no centro
  u8g2.drawXBM(60, 20, 8, 8, mic); // Centraliza 8x8: (128-8)/2 = 60
  
  // Status da gravação
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 45);
  u8g2.print("Tempo: ");
  u8g2.print((millis() - inicioGravacao) / 1000);
  u8g2.print("/");
  u8g2.print(RECORD_SECONDS);
  u8g2.print("s");
  
  u8g2.setCursor(0, 55);
  u8g2.print("Amostras: ");
  u8g2.print(indiceBuffer);
  u8g2.print("/");
  u8g2.print(BUFFER_SIZE);
  
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

// Função para exibir dados do MPU6050 no display
void mostrarDadosMPU() {
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 16);
  u8g2.print("MPU6050");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  
  // Acelerômetro
  u8g2.setCursor(0, 30);
  u8g2.print("Acc X:");
  u8g2.print(accelX, 1);
  u8g2.setCursor(0, 40);
  u8g2.print("Acc Y:");
  u8g2.print(accelY, 1);
  u8g2.setCursor(0, 50);
  u8g2.print("Acc Z:");
  u8g2.print(accelZ, 1);
  
  // Giroscópio
  u8g2.setCursor(64, 30);
  u8g2.print("Gyro X:");
  u8g2.print(gyroX, 1);
  u8g2.setCursor(64, 40);
  u8g2.print("Gyro Y:");
  u8g2.print(gyroY, 1);
  u8g2.setCursor(64, 50);
  u8g2.print("Gyro Z:");
  u8g2.print(gyroZ, 1);
  
  // Temperatura
  u8g2.setCursor(0, 60);
  u8g2.print("Temp:");
  u8g2.print(tempMPU, 1);
  u8g2.print("C");
  
  u8g2.sendBuffer();
}

// Função para obter dados do MPU6050 como string
String obterDadosMPUString() {
  String dados = "MPU6050 - ";
  dados += "Acc[" + String(accelX, 2) + "," + String(accelY, 2) + "," + String(accelZ, 2) + "] ";
  dados += "Gyro[" + String(gyroX, 2) + "," + String(gyroY, 2) + "," + String(gyroZ, 2) + "] ";
  dados += "Temp:" + String(tempMPU, 1) + "C";
  return dados;
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
      setHold(RENDER_COMANDO, 5000);
      
      if (cmd == "limpar") {
        mostrarMensagemStatus("Limpando credenciais...");
        setHold(RENDER_STATUS_MSG, 5000);
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
        setHold(RENDER_STATUS_WIFI, 5000);
        webSocket.sendTXT("Status exibido no display");
      } else if (cmd == "info") {
        mostrarInfoSistemaCompleto();
        setHold(RENDER_INFO_COMPLETO, 5000);
        webSocket.sendTXT("Informações do sistema exibidas");
      } else if (cmd == "audio") {
        // Grava e envia áudio
        gravarAudio();
        enviarAudioWebSocket();
        webSocket.sendTXT("Audio gravado e enviado em base64");
        // mantém tela de comando por 5s
        setHold(RENDER_COMANDO, 5000);
      } else if (cmd == "gravar") {
        // Apenas grava o áudio
        gravarAudio();
        webSocket.sendTXT("Audio gravado (use 'audio' para enviar)");
        setHold(RENDER_COMANDO, 5000);
      } else if (cmd == "reproduzir") {
        reproduzirAudioLocal();
        webSocket.sendTXT("Audio reproduzido localmente");
        setHold(RENDER_STATUS_MSG, 5000);
      } else if (cmd == "stream_on") {
        streamingAudio = true;
        streamPos = 0;
        streamChunkIdx = 0;
        webSocket.sendTXT("streaming_on");
        mostrarMensagemStatus("Streaming ON");
        setHold(RENDER_STATUS_MSG, 5000);
      } else if (cmd == "stream_off") {
        streamingAudio = false;
        // Envia eventual resto do chunk
        enviarChunkStreaming();
        webSocket.sendTXT("audio|fim");
        mostrarMensagemStatus("Streaming OFF");
        setHold(RENDER_STATUS_MSG, 5000);
      } else if (cmd == "botoes") {
        mostrarStatusBotoes();
        setHold(RENDER_STATUS_BOTOES, 5000);
        webSocket.sendTXT("Status dos botões exibido");
      } else if (cmd == "mpu") {
        mostrarDadosMPU();
        setHold(RENDER_MPU, 5000); // Mantém a tela de dados do MPU por 5s
        webSocket.sendTXT("Dados do MPU6050 exibidos");
      } else if (cmd.startsWith("mensagem|")) {
        int p1 = cmd.indexOf('|');
        if (p1 > 0) {
          String conteudo = cmd.substring(p1 + 1);
          mostrarMensagemStatus(conteudo);
          setHold(RENDER_STATUS_MSG, 5000);
          webSocket.sendTXT("Mensagem exibida: " + conteudo);
        } else {
          webSocket.sendTXT("erro:formato");
        }
      } else {
        // Comando não reconhecido
        mostrarMensagemStatus("Comando: " + cmd);
        setHold(RENDER_STATUS_MSG, 5000);
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
  WiFi.softAP("Relogio inteligente");              // Nome da rede Wi-Fi

  Serial.println("Portal de configuração ativo. Conecte-se à rede: Relogio inteligente");
  Serial.print("Acesse: http://");
  Serial.println(local_ip);

  dnsServer.start(DNS_PORT, "*", local_ip);     // Redireciona todo domínio para o IP do AP
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
// ===== FIM PORTAL DE CONFIGURAÇÃO =====

// Função para configurar e sincronizar NTP
void configurarNTP() {
  // Configura o fuso horário
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Aguarda a primeira sincronização
  Serial.println("Sincronizando com servidor NTP...");
  mostrarMensagemStatus("Sincronizando hora...");
  
  // Aguarda até 10 segundos para sincronização
  int tentativas = 0;
  while (time(nullptr) < 24 * 3600 && tentativas < 10) {
    Serial.print(".");
    delay(1000);
    tentativas++;
  }
  
  if (time(nullptr) > 24 * 3600) {
    ntpSincronizado = true;
    Serial.println("\nNTP sincronizado com sucesso!");
    
    // Mostra a hora atual
    time_t now = time(nullptr);
    struct tm *tmNow = localtime(&now);
    char buf[9];
    sprintf(buf, "%02d:%02d:%02d", tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);
  } else {
    Serial.println("\nFalha na sincronização NTP");
    mostrarMensagemStatus("Erro NTP - Hora local");
  }
}

// Função otimizada para atualizar o relógio com NTP
void atualizarRelogio(bool force) {
  // só consulta o RTC/NTP a cada segundo
  if (!force && millis() - lastClockMillis < 1000) return;
  lastClockMillis = millis();

  // Verifica se o NTP está sincronizado
  if (!ntpSincronizado) {
    lastClockString = "00:00:00";
    return;
  }

  time_t now = time(nullptr);
  struct tm *tmNow = localtime(&now);

  // Formata a hora (HH:MM:SS)
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);
  lastClockString = String(buf);
  
  // Removido: não resetar lastDisplayUpdate aqui para não forçar refresh a cada loop
}
// Função para exibir relógio analógico no centro da tela
void mostrarRelogio() {
  // Atualiza o relógio forçadamente
  atualizarRelogio(true);
  
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Extrai horas, minutos e segundos do lastClockString (formato HH:MM:SS)
  int horas = lastClockString.substring(0, 2).toInt();
  int minutos = lastClockString.substring(3, 5).toInt();
  int segundos = lastClockString.substring(6, 8).toInt();
  
  // Configurações do relógio analógico
  int centroX = 64;  // Centro horizontal da tela
  int centroY = 40;  // Centro vertical na área útil
  int raio = 22;     // Raio do relógio
  
  // Desenha o círculo do relógio
  u8g2.drawCircle(centroX, centroY, raio);
  
  // Desenha as marcas das horas (12, 3, 6, 9)
  for (int i = 0; i < 12; i++) {
    float angulo = i * PI / 6 - PI / 2; // -90° para começar no 12
    int x1 = centroX + cos(angulo) * (raio - 3);
    int y1 = centroY + sin(angulo) * (raio - 3);
    int x2 = centroX + cos(angulo) * (raio - 1);
    int y2 = centroY + sin(angulo) * (raio - 1);
    u8g2.drawLine(x1, y1, x2, y2);
  }
  
  // Calcula ângulos dos ponteiros (em radianos)
  float anguloHoras = ((horas % 12) * 30 + minutos * 0.5) * PI / 180 - PI / 2;
  float anguloMinutos = (minutos * 6) * PI / 180 - PI / 2;
  float anguloSegundos = (segundos * 6) * PI / 180 - PI / 2;
  
  // Desenha ponteiro das horas (grosso e curto)
  int xHoras = centroX + cos(anguloHoras) * (raio - 8);
  int yHoras = centroY + sin(anguloHoras) * (raio - 8);
  u8g2.drawLine(centroX, centroY, xHoras, yHoras);
  u8g2.drawLine(centroX + 1, centroY, xHoras + 1, yHoras);
  
  // Desenha ponteiro dos minutos (médio)
  int xMinutos = centroX + cos(anguloMinutos) * (raio - 3);
  int yMinutos = centroY + sin(anguloMinutos) * (raio - 3);
  u8g2.drawLine(centroX, centroY, xMinutos, yMinutos);
  
  // Desenha ponteiro dos segundos (fino e longo)
  int xSegundos = centroX + cos(anguloSegundos) * (raio - 1);
  int ySegundos = centroY + sin(anguloSegundos) * (raio - 1);
  u8g2.drawPixel(centroX, centroY);
  u8g2.drawLine(centroX, centroY, xSegundos, ySegundos);
  
  // Desenha ponto central
  u8g2.drawDisc(centroX, centroY, 2);
  
  u8g2.sendBuffer();
}
void setup() {
  pinMode(ledPin, OUTPUT);
  // Pinos de joystick removidos – controle será por giroscópio
  
  // Configuração do microfone
  pinMode(MIC_PIN, INPUT);
  
  // KEYPAD: configuração dos pinos da matriz 4x4 é feita automaticamente pela biblioteca Keypad
  
  Serial.begin(115200);
  randomSeed(esp_random());

  // Debug dos thresholds dos botões
  Serial.println("=== CONFIGURAÇÃO DOS BOTÕES ===");
  Serial.printf("LONG_PRESS_THRESHOLD: %dms\n", LONG_PRESS_THRESHOLD);
  Serial.println("=============================");

  // Inicia barramento I2C nos pinos 21 (SDA) e 22 (SCL)
  Wire.begin(21, 22);

  // Inicializa display OLED
  u8g2.begin();
  mostrarStatusInicial();
  
  // Aguarda estabilização do barramento I2C
  delay(100);

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

  // Configura e sincroniza NTP
  configurarNTP();

  // Configurações do WebSocket
  webSocket.setReconnectInterval(5000);
  webSocket.setAuthorization("esp", "neurelix");
  webSocket.setExtraHeaders("Sec-WebSocket-Extensions:");
  webSocket.onEvent(webSocketEvent);
  webSocket.beginSSL("esp-conecta.neurelix.com.br", 443, "/");

  // Inicializa o MPU6050
  Serial.println("Inicializando MPU6050...");
  
  // Scanner I2C para diagnosticar
  Serial.println("Scanner I2C:");
  byte found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("I2C encontrado em 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) {
    Serial.println("Nenhum dispositivo I2C encontrado!");
    mpuDisponivel = false;
  } else {
    // Tenta inicializar o MPU6050
    if (!mpu.begin(0x68, &Wire)) {
      Serial.println("Falha ao inicializar o MPU6050");
      mpuDisponivel = false;
    } else {
      mpuDisponivel = true;
      Serial.println("MPU6050 inicializado com sucesso!");
      
      // Configura o MPU6050
      mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
      mpu.setGyroRange(MPU6050_RANGE_500_DEG);
      mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }
  }
}

void loop() {
  // Verifica os botões da matriz 4x4 (sem bloquear)
  verificarBotoes();
  
  // Atualiza relógio primeiro (prioridade máxima)
  atualizarRelogio(false);
  
  // Lê dados do MPU6050 periodicamente
  if (mpuDisponivel && millis() - lastMPURead > mpuReadInterval) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    
    // Armazena dados nas variáveis globais
    accelX = a.acceleration.x;
    accelY = a.acceleration.y;
    accelZ = a.acceleration.z;
    gyroX = g.gyro.x;
    gyroY = g.gyro.y;
    gyroZ = g.gyro.z;
    tempMPU = temp.temperature;
    
    lastMPURead = millis();
    
    // Debug: mostra dados a cada 10 leituras (1 segundo)
    static int debugCounter = 0;
    if (++debugCounter >= 10) {
      Serial.printf("MPU6050 - Acc [m/s^2]: %.2f %.2f %.2f | Gyro [°/s]: %.2f %.2f %.2f | Temp: %.1f°C\n",
                    accelX, accelY, accelZ, gyroX, gyroY, gyroZ, tempMPU);
      debugCounter = 0;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();

    // ===== Streaming em tempo real =====
    if (streamingAudio) {
      unsigned long nowMicros = micros();
      
      // Calcula o tempo desde a última amostra (trata overflow do micros)
      unsigned long tempoDesdeUltimaAmostra;
      if (nowMicros >= lastStreamSampleMicros) {
        tempoDesdeUltimaAmostra = nowMicros - lastStreamSampleMicros;
      } else {
        // Overflow ocorreu, calcula tempo restante
        tempoDesdeUltimaAmostra = (0xFFFFFFFF - lastStreamSampleMicros) + nowMicros;
      }
      
      unsigned long tempoEntreAmostras = 1000000UL / SAMPLE_RATE; // 125μs para 8kHz
      
      // Debug: mostra timing a cada 1000 amostras
      static unsigned long amostrasDebug = 0;
      if (amostrasDebug % 1000 == 0) {
        Serial.printf("Timing: %luμs desde última, intervalo: %luμs | Last: %lu | Now: %lu\n", 
                     tempoDesdeUltimaAmostra, tempoEntreAmostras, lastStreamSampleMicros, nowMicros);
        amostrasDebug = 0;
      }
      
      if (tempoDesdeUltimaAmostra >= tempoEntreAmostras) {
        lastStreamSampleMicros = nowMicros;

        // Captura amostra de áudio e converte para 8 bits
        int leitura = analogRead(MIC_PIN); // 0 a 4095
        uint8_t amostra = leitura >> 4; // Converte para 8 bits (0 a 255)
        streamChunkBuf[streamPos++] = amostra;
        
        amostrasDebug++;

        // Debug: mostra amostra capturada a cada 100
        if (amostrasDebug % 100 == 0) {
          Serial.printf("Amostra capturada: %d (raw: %d) | Buffer: %d/%d | Micros: %lu\n", 
                       amostra, leitura, streamPos, AUDIO_CHUNK_SIZE, nowMicros);
        }

        // Envia chunk quando estiver cheio
        if (streamPos >= AUDIO_CHUNK_SIZE) {
          Serial.printf("Buffer cheio! Enviando chunk #%d\n", streamChunkIdx);
          enviarChunkStreaming();
        }
      } else if (amostrasDebug == 0) {
        // Força primeira amostra se nenhuma foi capturada ainda
        Serial.println("Forçando primeira amostra...");
        lastStreamSampleMicros = nowMicros;
        
        int leitura = analogRead(MIC_PIN);
        uint8_t amostra = leitura >> 4;
        streamChunkBuf[streamPos++] = amostra;
        amostrasDebug++;
        
        Serial.printf("Primeira amostra forçada: %d (raw: %d) | Buffer: %d/%d\n", 
                     amostra, leitura, streamPos, AUDIO_CHUNK_SIZE);
      }
      
      // Monitoramento de status do streaming a cada 5 segundos
      static unsigned long lastStreamStatus = 0;
      if (millis() - lastStreamStatus > 5000) {
        Serial.printf("Status Streaming: Ativo | Chunks: %d | Amostras: %d | Buffer: %d/%d\n",
                     streamChunkIdx, streamChunkIdx * AUDIO_CHUNK_SIZE, streamPos, AUDIO_CHUNK_SIZE);
        lastStreamStatus = millis();
      }
    }

    if (millis() - lastPing > pingInterval) {
      webSocket.sendTXT("ping");
      Serial.println("ping");
      lastPing = millis();
    }
    
    // Atualiza display periodicamente
    if (millis() - lastDisplayUpdate > displayUpdateInterval) {
      if (millis() < actionHoldUntil && heldRender != RENDER_NONE) {
        // Mantém a tela da ação por 5s, mas re-renderiza para atualizar barra/relogio
        renderHeldView();
      } else if (gravandoAudio || enviandoAudio) {
        mostrarStatusAudio();
        Serial.println("Display: Status Audio");
      } else if (streamingAudio) {
        mostrarStatusBotoes();
        //Serial.println("Display: Status Botoes");
      } else if (lastCommand.length() > 0 && (millis() - lastCommandMillis) < commandDisplayDuration) {
        mostrarComando(lastCommand);
      } else {
        if (lastCommand.length() > 0 && (millis() - lastCommandMillis) >= commandDisplayDuration) {
          lastCommand = "";
        }
        mostrarRelogio();
        Serial.println("Display: Relogio atualizado - " + lastClockString);
        heldRender = RENDER_NONE; // Libera render mantido
      }
      lastDisplayUpdate = millis();
    }
  } else {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Atualiza relógio mesmo no portal de configuração
    atualizarRelogio(false);
    
    // Atualiza display no portal de configuração
    if (millis() - lastDisplayUpdate > displayUpdateInterval) {
      if (millis() < actionHoldUntil && heldRender != RENDER_NONE) {
        renderHeldView();
      } else if (lastCommand.length() > 0 && (millis() - lastCommandMillis) < commandDisplayDuration) {
        mostrarComando(lastCommand);
      } else {
        if (lastCommand.length() > 0 && (millis() - lastCommandMillis) >= commandDisplayDuration) {
          lastCommand = "";
        }
        mostrarRelogio();
        heldRender = RENDER_NONE;
      }
      lastDisplayUpdate = millis();
    }
  }
}

// Função para obter hora formatada atual com NTP
String obterHoraAtual() {
  if (!ntpSincronizado) {
    return "00:00:00";
  }
  
  time_t now = time(nullptr);
  struct tm *tmNow = localtime(&now);
  
  return String(tmNow->tm_hour < 10 ? "0" : "") + String(tmNow->tm_hour) + ":" + 
         String(tmNow->tm_min < 10 ? "0" : "") + String(tmNow->tm_min) + ":" + 
         String(tmNow->tm_sec < 10 ? "0" : "") + String(tmNow->tm_sec);
}
