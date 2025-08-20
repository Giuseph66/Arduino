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
    u8g2.drawXBM(2, 1, 8, 8, mic);
  }

  // Desenha ícone Wi-Fi no lado esquerdo (após o microfone se ativo)
  int wifiX = (gravandoAudio || streamingAudio) ? 12 : 2; // Ajusta posição baseado no microfone
  u8g2.drawXBM(wifiX, 1, 8, 8, wifi_icon_bits);
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
  char tecla = teclado.getKey();

  if (tecla != NO_KEY) {
    // Nova tecla pressionada
    teclaAtual = tecla;
    tempoInicio = millis();
    pressionando = true;
    
    // Exibe no Serial para debug
    int index = mapearTecla(tecla);
    if (index != -1) {
      Serial.print("Botão ");
      Serial.print(nomesTeclas[index]);
      Serial.println(" pressionado");
    }
  }

  if (pressionando && teclado.getState() == HOLD) {
    // Botão sendo segurado - pode implementar ações de longo pressionamento
    unsigned long duracao = millis() - tempoInicio;
    
    // Exemplo: após 2 segundos de pressão contínua
    if (duracao >= LONG_PRESS_THRESHOLD && duracao < LONG_PRESS_THRESHOLD + 100) {
      int index = mapearTecla(teclaAtual);
      if (index != -1) {
        Serial.print("Ação de longo pressionamento para ");
        Serial.println(nomesTeclas[index]);
        
        // Implementar ações específicas para cada botão
        executarAcaoBotao(index, true); // true = longo pressionamento
      }
    }
  }

  if (pressionando && teclado.getState() == RELEASED) {
    // Botão foi solto
    pressionando = false;
    unsigned long duracao = millis() - tempoInicio;

    int index = mapearTecla(teclaAtual);

    if (index != -1) {
      Serial.print("Botão ");
      Serial.print(nomesTeclas[index]);

      if (duracao >= SHORT_PRESS_THRESHOLD) {
        Serial.println(" → Clique longo");
        executarAcaoBotao(index, true);
      } else {
        Serial.println(" → Clique curto");
        executarAcaoBotao(index, false);
      }
    }

    teclaAtual = NO_KEY;
  }
}

// Função auxiliar para mapear a tecla para o índice S1–S16
int mapearTecla(char tecla) {
  switch(tecla) {
    case '1': return 0;
    case '2': return 1;
    case '3': return 2;
    case '4': return 3;
    case '5': return 5;
    case '6': return 4;
    case '7': return 6;
    case '8': return 7;
    case '9': return 9;
    case 'A': return 8;
    case 'B': return 10;
    case 'C': return 11;
    case 'D': return 13;
    case 'E': return 12;
    case 'F': return 14;
    case 'G': return 15;
    default: return -1;
  }
}

// Função para executar ações específicas para cada botão
void executarAcaoBotao(int index, bool longPress) {
  switch(index) {
    case 0: // S1
      if (longPress) {
        mostrarInfoSistema();
      } else {
        mostrarRelogio(); // Exibe relógio no centro
      }
      break;
      
    case 1: // S2
      if (longPress) {
        mostrarInfoSistemaCompleto();
      } else {
        mostrarStatusAudio();
      }
      break;
      
    case 2: // S3
      if (longPress) {
        mostrarStatusBotoes();
      } else {
        mostrarMensagemStatus("Botão S3 - Clique curto");
      }
      break;
      
    case 3: // S4
      if (longPress) {
        mostrarMensagemStatus("Botão S4 - Clique longo");
      } else {
        mostrarMensagemStatus("Botão S4 - Clique curto");
      }
      break;
      
    case 4: // S5
      if (longPress) {
        mostrarMensagemStatus("Botão S5 - Clique longo");
      } else {
        mostrarMensagemStatus("Botão S5 - Clique curto");
      }
      break;
      
    case 5: // S6
      if (longPress) {
        mostrarMensagemStatus("Botão S6 - Clique longo");
      } else {
        mostrarMensagemStatus("Botão S6 - Clique curto");
      }
      break;
      
    case 6: // S7
      if (longPress) {
        mostrarMensagemStatus("Botão S7 - Clique longo");
      } else {
        mostrarMensagemStatus("Botão S7 - Clique curto");
      }
      break;
      
    case 7: // S8
      if (longPress) {
        mostrarMensagemStatus("Botão S8 - Clique longo");
      } else {
        mostrarMensagemStatus("Botão S8 - Clique curto");
      }
      break;
      
    case 8: // S9
      if (longPress) {
        mostrarMensagemStatus("Botão S9 - Clique longo");
      } else {
        mostrarMensagemStatus("Botão S9 - Clique curto");
      }
      break;
      
    case 9: // S10
      if (longPress) {
        mostrarMensagemStatus("Botão S10 - Clique longo");
      } else {
        mostrarIconeCentro(giroscopico, 32, 32, "Giroscopico");
      }
      break;
      
    case 10: // S11
      if (longPress) {
        mostrarMensagemStatus("S11 - Clique longo");
      } else {  
        mostrarIconeCentro(coffe, 32, 32, "Cafe!");
      }
      break;
      
    case 11: // S12
      if (longPress) {
        mostrarMensagemStatus("Botão S12 - Clique longo");
      } else {
        mostrarIconeCentro(apple, 32, 32, "Maca!");
      }
      break;
      
    case 12: // S13
      if (longPress) {
        mostrarMensagemStatus("Botão S13 - Clique longo");
      } else {
        mostrarIconeCentro(android, 32, 32, "Android!");
      }
      break;
      
    case 13: // S14
      if (longPress) {
        mostrarMensagemStatus("Botão S14 - Clique longo");
      } else {
        mostrarIconeCentro(whatts, 32, 32, "Whatts");
      }
      break;
      
    case 14: // S15 - Iniciar/Encerrar áudio (mantém funcionalidade original)
      if (longPress) {
        // Ação de longo pressionamento para S15
        if (streamingAudio) {
          // Encerra streaming
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
        } else {
          mostrarMensagemStatus("S15 - Clique longo");
        }
      } else {
        // Clique curto - toggle do streaming
        if (streamingAudio) {
          // Encerra streaming
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
        } else if (gravandoAudio) {
          gravandoAudio = false;
          mostrarMensagemStatus("Gravacao encerrada");
        } else {
          // Não estava ativo → inicia streaming
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
          Serial.printf("Tempo entre amostras: %lu μs\n", 1000000UL / SAMPLE_RATE);
          Serial.printf("Micros inicial: %lu\n", lastStreamSampleMicros);
          Serial.println("========================");
          
          mostrarMensagemStatus("Audio iniciado");
          webSocket.sendTXT("streaming_on");
        }
      }
      break;
      
    case 15: // S16 - Encerrar áudio (mantém funcionalidade original)
      if (longPress) {
        // Ação de longo pressionamento para S16
        if (streamingAudio || gravandoAudio) {
          if (streamingAudio) {
            // Encerra streaming
            Serial.println("=== STREAMING ENCERRADO (S16 - Longo) ===");
            Serial.printf("Total de chunks enviados: %d\n", streamChunkIdx);
            Serial.printf("Total de amostras capturadas: %d\n", streamChunkIdx * AUDIO_CHUNK_SIZE);
            Serial.printf("Tempo total de streaming: %.2f segundos\n", 
                         (float)(streamChunkIdx * AUDIO_CHUNK_SIZE) / SAMPLE_RATE);
            Serial.println("=====================================");
          }
          
          streamingAudio = false;
          gravandoAudio = false;
          enviarChunkStreaming();
          webSocket.sendTXT("audio|fim");
          mostrarMensagemStatus("Audio encerrado (longo)");
        } else {
          mostrarMensagemStatus("S16 - Clique longo");
        }
      } else {
        // Clique curto - encerra áudio
        if (streamingAudio) {
          // Encerra streaming
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
        }
        
        if (gravandoAudio) {
          gravandoAudio = false;
          mostrarMensagemStatus("Gravacao encerrada");
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
  mostrarMensagemStatus("Sincronizando NTP...");
  
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
    mostrarMensagemStatus("Hora: " + String(buf));
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
  
  // Sempre força atualização do display quando o relógio é atualizado
  lastDisplayUpdate = 0; // Força atualização imediata
}

// Função para exibir relógio no centro da tela
void mostrarRelogio() {
  // Atualiza o relógio forçadamente
  atualizarRelogio(true);
  
  u8g2.clearBuffer();
  drawNotificationBar();
  
  // Relógio grande no centro da tela
  u8g2.setFont(u8g2_font_10x20_tf);
  String horaAtual = lastClockString;
  int larguraHora = u8g2.getUTF8Width(horaAtual.c_str());
  int posX = (128 - larguraHora) / 2;
  int posY = 45; // Centraliza verticalmente na área útil
  
  u8g2.setCursor(posX, posY);
  u8g2.print(horaAtual);
  
  // Texto explicativo abaixo
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 60);
  u8g2.print("Relogio do Sistema");
  
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

  // Configura e sincroniza NTP
  configurarNTP();

  // Configurações do WebSocket
  webSocket.setReconnectInterval(5000);
  webSocket.setAuthorization("esp", "neurelix");
  webSocket.setExtraHeaders("Sec-WebSocket-Extensions:");
  webSocket.onEvent(webSocketEvent);
  webSocket.beginSSL("esp-conecta.neurelix.com.br", 443, "/");
}

void loop() {
  // Verifica os botões da matriz 4x4 (sem bloquear)
  verificarBotoes();
  
  // Atualiza relógio primeiro (prioridade máxima)
  atualizarRelogio(true);
  
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
      if (gravandoAudio || enviandoAudio) {
        mostrarStatusAudio();
        Serial.println("Display: Status Audio");
      } else if (streamingAudio) {
        mostrarStatusBotoes();
        Serial.println("Display: Status Botoes");
      } else if (lastCommand.length() == 0) {
        // Sempre mostra o relógio quando não há outras atividades
        mostrarRelogio();
        Serial.println("Display: Relogio atualizado - " + lastClockString);
      }
      lastDisplayUpdate = millis();
    }
  } else {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Atualiza relógio mesmo no portal de configuração
    atualizarRelogio(true);
    
    // Atualiza display no portal de configuração
    if (millis() - lastDisplayUpdate > displayUpdateInterval) {
      if (lastCommand.length() == 0) {
        // Se não há comando ativo, mostra o relógio
        mostrarRelogio();
      } else {
        mostrarMensagemStatus("Portal de configuracao");
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
