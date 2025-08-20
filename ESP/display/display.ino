#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
// FIRST_EDIT: inclui bibliotecas OLED
#include <Wire.h>
#include <U8g2lib.h>

String ssid;
String password;
String emailLogin;
Preferences preferences;
WebServer server(80);

String randomId; // parte aleatória gerada apenas uma vez
const char* host = "fastshii.neurelix.com.br";  // sem "https://"
const int ledPin = 2;  // Pino do LED onboard (geralmente D2)
const int tempoGiro = 1000; // tempo de giro do motor em ms
// Adicionar pinos do joystick
const int pinX = 34;
const int pinY = 35;
const int pinSW = 25;
unsigned long lastPing = 0;
const unsigned long pingInterval = 50000; // 50 segundos

WebSocketsClient webSocket;

DNSServer dnsServer;
const byte DNS_PORT = 53;
// SECOND_EDIT: cria objeto U8g2 para display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);

// Variáveis para controle do display
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 200; // Atualiza display a cada 200ms (mesmo do joystick)
String lastCommand = "";
String statusMessage = "";
// Variáveis para o joystick
int joystickX = 0;
int joystickY = 0;
bool joystickPressed = false;
unsigned long lastJoystickRead = 0;
const unsigned long joystickReadInterval = 200; // Lê joystick a cada 200ms
// Variável para controlar modo joystick ativo
bool modoJoystickAtivo = false;
// Variáveis para o jogo da cobrinha
bool jogoAtivo = false;
const int TAMANHO_MAX_COBRA = 20;
int LARGURA_TELA = 21.5;  // Mudado de const para int para permitir modificação
int ALTURA_TELA = 7.5;    // Mudado de const para int para permitir modificação

struct Posicao {
  int x, y;
};

Posicao cobra[TAMANHO_MAX_COBRA];
int tamanhoCobra = 3;
int direcaoX = 1, direcaoY = 0; // Começa indo para direita
Posicao fruta;
unsigned long ultimoMovimento = 0;
const unsigned long velocidadeJogo = 300; // 300ms entre movimentos
int pontuacao = 0;

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
  
  // Título em fonte média
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("ESP32 Display");
  
  // Subtítulo
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 25);
  u8g2.print("Iniciando sistema...");
  u8g2.setCursor(0, 35);
  u8g2.print("Aguarde...");
  
  u8g2.sendBuffer();
}

void mostrarStatusWiFi() {
  u8g2.clearBuffer();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("Status WiFi");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.setCursor(0, 25);
    u8g2.print("● Conectado");
    u8g2.setCursor(0, 35);
    u8g2.print("IP: ");
    u8g2.print(WiFi.localIP().toString());
    u8g2.setCursor(0, 45);
    u8g2.print("RSSI: ");
    u8g2.print(WiFi.RSSI());
    u8g2.print(" dBm");
    u8g2.setCursor(0, 55);
    u8g2.print("SSID: ");
    u8g2.print(ssid);
  } else {
    u8g2.setCursor(0, 25);
    u8g2.print("○ Desconectado");
    u8g2.setCursor(0, 35);
    u8g2.print("Portal ativo");
    u8g2.setCursor(0, 45);
    u8g2.print("192.168.10.1");
    u8g2.setCursor(0, 55);
    u8g2.print("Rede: Porta automatica");
  }
  u8g2.sendBuffer();
}

void mostrarComando(const String& comando) {
  u8g2.clearBuffer();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("Comando:");
  
  // Texto do comando com quebra automática
  String texto = "> " + comando;
  drawWrappedText(texto.c_str(), 0, 25, 128);
  
  // Timestamp
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 60);
  u8g2.print("Recebido agora");
  
  u8g2.sendBuffer();
  lastCommand = comando;
}

void mostrarTexto(const String& texto) {
  mostrarComando(texto);
}

void mostrarMensagemStatus(const String& mensagem) {
  u8g2.clearBuffer();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("Status:");
  
  // Mensagem com quebra automática
  String texto = "● " + mensagem;
  drawWrappedText(texto.c_str(), 0, 25, 128);
  
  u8g2.sendBuffer();
  statusMessage = mensagem;
}

void mostrarInfoSistema() {
  u8g2.clearBuffer();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("Sistema");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 25);
  u8g2.print("ID: ");
  u8g2.print(randomId);
  
  u8g2.setCursor(0, 35);
  u8g2.print("Uptime: ");
  u8g2.print(millis() / 1000);
  u8g2.print("s");
  
  u8g2.setCursor(0, 45);
  u8g2.print("Mem: ");
  u8g2.print(ESP.getFreeHeap());
  u8g2.print(" bytes");
  
  u8g2.setCursor(0, 55);
  u8g2.print("Email: ");
  u8g2.print(emailLogin);
  
  u8g2.sendBuffer();
}

void mostrarJoystick() {
  u8g2.clearBuffer();
  
  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("Joystick Ativo");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 25);
  u8g2.print("X: ");
  u8g2.print(joystickX);
  
  u8g2.setCursor(0, 35);
  u8g2.print("Y: ");
  u8g2.print(joystickY);
  
  u8g2.setCursor(0, 45);
  u8g2.print("Botao: ");
  u8g2.print(joystickPressed ? "PRESSIONADO" : "SOLTO");
  
  // Desenha um pequeno indicador visual da posição
  u8g2.setCursor(0, 55);
  u8g2.print("Pos: ");
  if (joystickX < 1000) u8g2.print("L");
  else if (joystickX > 3000) u8g2.print("R");
  else u8g2.print("C");
  
  if (joystickY < 1000) u8g2.print("U");
  else if (joystickY > 3000) u8g2.print("D");
  else u8g2.print("C");
  
  u8g2.sendBuffer();
}

void lerJoystick() {
  if (millis() - lastJoystickRead > joystickReadInterval) {
    joystickX = analogRead(pinX);
    joystickY = analogRead(pinY);
    joystickPressed = digitalRead(pinSW) == LOW;
    
    // Debug no Serial
    Serial.print("X: ");
    Serial.print(joystickX);
    Serial.print(" | Y: ");
    Serial.print(joystickY);
    Serial.print(" | Botao: ");
    Serial.println(joystickPressed ? "PRESSIONADO" : "SOLTO");
    
    lastJoystickRead = millis();
  }
}

void inicializarJogo() {
  // Inicializa cobra no centro
  for (int i = 0; i < TAMANHO_MAX_COBRA; i++) {
    cobra[i].x = LARGURA_TELA / 2;
    cobra[i].y = ALTURA_TELA / 2;
  }
  tamanhoCobra = 3;
  direcaoX = 1;
  direcaoY = 0;
  pontuacao = 0;
  gerarNovaFruta();
  jogoAtivo = true;
}

void gerarNovaFruta() {
  bool posicaoValida;
  do {
    posicaoValida = true;
    fruta.x = random(LARGURA_TELA);
    fruta.y = random(ALTURA_TELA);
    
    // Verifica se não está na cobra
    for (int i = 0; i < tamanhoCobra; i++) {
      if (fruta.x == cobra[i].x && fruta.y == cobra[i].y) {
        posicaoValida = false;
        break;
      }
    }
  } while (!posicaoValida);
}

void moverCobra() {
  // Calcula nova posição da cabeça
  int novaX = cobra[0].x + direcaoX;
  int novaY = cobra[0].y + direcaoY;
  
  // Teleporte nas bordas
  if (novaX < 0) novaX = LARGURA_TELA - 1;
  if (novaX >= LARGURA_TELA) novaX = 0;
  if (novaY < 0) novaY = ALTURA_TELA - 1;
  if (novaY >= ALTURA_TELA) novaY = 0;
  
  // Verifica colisão com o próprio corpo
  for (int i = 0; i < tamanhoCobra; i++) {
    if (novaX == cobra[i].x && novaY == cobra[i].y) {
      jogoAtivo = false;
      return;
    }
  }
  
  // Move o corpo (exceto cabeça)
  for (int i = tamanhoCobra - 1; i > 0; i--) {
    cobra[i] = cobra[i-1];
  }
  
  // Move a cabeça
  cobra[0].x = novaX;
  cobra[0].y = novaY;
  
  // Verifica se comeu a fruta
  if (cobra[0].x == fruta.x && cobra[0].y == fruta.y) {
    if (tamanhoCobra < TAMANHO_MAX_COBRA) {
      tamanhoCobra++;
    }
    pontuacao += 10;
    gerarNovaFruta();
  }
}

void mostrarJogo() {
  u8g2.clearBuffer();
  
  // Título e pontuação compactos
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("Snake ");
  u8g2.print(pontuacao);
  
  // Desenha o jogo ocupando mais espaço
  int offsetX = 0;
  int offsetY = 20;
  
  // Desenha bordas
  u8g2.drawFrame(offsetX, offsetY, LARGURA_TELA * 6, ALTURA_TELA * 6);
  
  // Desenha a cobra
  for (int i = 0; i < tamanhoCobra; i++) {
    int x = offsetX + cobra[i].x * 6 + 1;
    int y = offsetY + cobra[i].y * 6 + 1;
    if (i == 0) {
      // Cabeça (quadrado cheio)
      u8g2.drawBox(x, y, 4, 4);
    } else {
      // Corpo (quadrado vazio)
      u8g2.drawFrame(x, y, 4, 4);
    }
  }
  
  // Desenha a fruta
  int frutaX = offsetX + fruta.x * 6 + 1;
  int frutaY = offsetY + fruta.y * 6 + 1;
  u8g2.drawDisc(frutaX + 2, frutaY + 2, 2);
  
  u8g2.sendBuffer();
}

void mostrarGameOver() {
  u8g2.clearBuffer();
  
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 15);
  u8g2.print("GAME OVER");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 30);
  u8g2.print("Score: ");
  u8g2.print(pontuacao);
  
  u8g2.setCursor(0, 45);
  u8g2.print("Botao para reiniciar");
  
  u8g2.sendBuffer();
}

void processarControlesJoystick() {
  // Controles do jogo
  if (jogoAtivo) {
    // Evita inverter direção (morte instantânea)
    if (joystickX < 1000 && direcaoX != 1) { // Esquerda
      direcaoX = -1;
      direcaoY = 0;
    } else if (joystickX > 3000 && direcaoX != -1) { // Direita
      direcaoX = 1;
      direcaoY = 0;
    } else if (joystickY < 1000 && direcaoY != 1) { // Cima
      direcaoX = 0;
      direcaoY = -1;
    } else if (joystickY > 3000 && direcaoY != -1) { // Baixo
      direcaoX = 0;
      direcaoY = 1;
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Desconectado do servidor WebSocket");
      mostrarMensagemStatus("WebSocket desconectado");
      modoJoystickAtivo = false;
      jogoAtivo = false;
      break;
    case WStype_CONNECTED:
      Serial.println("Conectado ao servidor WebSocket");
      mostrarMensagemStatus("WebSocket conectado");
      modoJoystickAtivo = false;
      jogoAtivo = false;
      {
        String identificador = ssid + "|" + password + "|" + emailLogin + "|" + randomId;
        webSocket.sendTXT(identificador);
      }
      break;
    case WStype_TEXT:
      Serial.printf("Comando recebido: %s\n", payload);

      String cmd = String((char*)payload);

      // Desativa modos para qualquer comando
      modoJoystickAtivo = false;
      jogoAtivo = false;

      // Exibe comando no display
      mostrarComando(cmd);
      
      fecharJogo();
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
        mostrarInfoSistema();
        webSocket.sendTXT("Informações do sistema exibidas");
      } else if (cmd == "joystick") {
        modoJoystickAtivo = true;
        mostrarJoystick();
        webSocket.sendTXT("Modo joystick ativado - monitorando movimentos");
      } else if (cmd == "snake") {
        inicializarJogo();
        mostrarJogo();
        webSocket.sendTXT("Jogo Snake iniciado! Use joystick para jogar");
      } else if (cmd.startsWith("alt|")) {
        String alt = cmd.substring(4);
        ALTURA_TELA = alt.toInt();
        webSocket.sendTXT("Altura definida como: " + alt);
      } else if (cmd.startsWith("larg|")) {
        String larg = cmd.substring(5); 
        LARGURA_TELA = larg.toInt();
        webSocket.sendTXT("Largura definida como: " + larg);
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
void fecharJogo() {
  jogoAtivo = false;
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
  // Configurar pinos do joystick
  pinMode(pinSW, INPUT_PULLUP); // botão é ativo em nível baixo

  Serial.begin(115200);
  randomSeed(esp_random());
  
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
  // Lê joystick periodicamente
  lerJoystick();
  
  // Verifica reinício do jogo (fora do jogo ativo)
  if (!jogoAtivo && joystickPressed && ultimoMovimento > 0) {
    delay(200); // Debounce
    inicializarJogo();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();

    if (millis() - lastPing > pingInterval) {
      webSocket.sendTXT("ping");
      Serial.println("ping");
      lastPing = millis();
    }
    
    // Atualiza display periodicamente
    if (millis() - lastDisplayUpdate > displayUpdateInterval) {
      if (jogoAtivo) {
        // Processa controles do jogo
        processarControlesJoystick();
        
        // Move a cobra
        if (millis() - ultimoMovimento > velocidadeJogo) {
          moverCobra();
          ultimoMovimento = millis();
        }
        
        mostrarJogo();
      } else if (modoJoystickAtivo) {
        mostrarJoystick();
      } else if (lastCommand.length() == 0) {
        mostrarStatusWiFi();
      }
      lastDisplayUpdate = millis();
    }
    
    // Mostra game over se necessário
    if (!jogoAtivo && ultimoMovimento > 0) {
      mostrarGameOver();
    }
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
