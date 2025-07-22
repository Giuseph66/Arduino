#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

String ssid;
String password;
String emailLogin;
Preferences preferences;
WebServer server(80);

String randomId; // parte aleatória gerada apenas uma vez
const char* host = "esp-conecta.neurelix.com.br";  // sem "https://"
const int ledPin = 2;  // Pino do LED onboard (geralmente D2)
// const int motorIN1 = 26;  // GPIO para IN1 da ponte H (REMOVIDO)
// const int motorIN2 = 27;  // GPIO para IN2 da ponte H (REMOVIDO)
// const int tempoGiro = 1000; // tempo de giro do motor em ms (REMOVIDO)
unsigned long lastPing = 0;
const unsigned long pingInterval = 50000; // 50 segundos
// ADIÇÃO: pinos dos servos e variáveis da sequência
const int servoHalfPin = 18;   // Servo quantidade 1/4
const int servoQuarterPin = 19; // Servo quantidade 2/4

LiquidCrystal_I2C lcd(0x27, 16, 2);
String generatedSequence = "";
String user = "";
unsigned long timestampSequence = 0;
unsigned long countdownStart = 0;
int countdownValue = 60;
bool countdownActive = false;

Servo servoHalf;
Servo servoQuarter;

// === Variáveis de preparo / cooldown ===
bool brewActive = false;
unsigned long brewStart = 0;
int brewPrevRemain = 61; // valor impossível para forçar primeira atualização
String brewMessage = "";
bool brewRandomShown = false;

// === Variáveis para rolagem não-bloqueante ===
bool scrollActive = false;
String scrollText = "";
unsigned long scrollLast = 0;
int scrollPos = 0;
const int scrollSpeed = 300; // ms entre passos
const int displayWidth = 16;

// === Função para verificar se a máquina está ocupada ===
bool isBusy() {
  return brewActive || generatedSequence.length() > 0;
}

WebSocketsClient webSocket;
String motivacao[]={
  "Todo grande projeto comeca com um pequeno gole de cafe!",
  "Que suas ideias fluam tao intensas quanto o aroma do cafe!",
  "Sinta o poder extra que so uma boa dose de cafe traz!",
  "Desperte seu melhor com cada xicara de cafe!",
  "Encontre foco e inspiracao em cada golada de cafe!",
  "Deixe o ritmo do cafe acelerar suas conquistas!",
  "Transforme energia em realizacao!",
  "Novas ideias nascem a cada gole de cafe!",
  "Renove sua motivacao com esse combustivel especial!",
  "Cafe e determinacao: a combinacao perfeita!",
  "O impulso que faltava para dar o proximo passo!",
  "A cada xicara, um novo desafio e superado!",
  "Cafe: o combustivel dos sonhos e conquistas!",
  "Programar sem cafe e como compilar sem codigo!",
  "Cafe: debugando bugs e desbloqueando solucoes!",
  "Cafe quente, mente brilhante!",
  "Um cafe por dia, mil linhas de codigo resolvidas!",
  "O segredo do sucesso tem aroma de cafe!",
  "Cafe: o parceiro fiel de todo desenvolvedor!",
  "Codando sonhos, um gole de cada vez!",
  "Cafe: onde as melhores ideias se revelam!",
  "Cafe e codigo: a dupla dinamica do sucesso!"
};

DNSServer dnsServer;
const byte DNS_PORT = 53;

// === Protótipos das funções de Carrossel ===
void exibirCarrossel(String mensagens[], int numMensagens, int tempoExibicao = 3000);
void exibirCarrosselDuasLinhas(String mensagens[], int numMensagens, int tempoExibicao = 3000);
void exibirCarrosselRolagem(String texto, int tempoExibicao = 3000, int velocidadeRolagem = 300, bool desligarBacklight = true);
// Protótipos rolagem não-bloqueante
void iniciarScroll(String txt);
void atualizarScroll();

// Função para piscar o LED 3 vezes
void piscarLed(int vezes) {
  for (int i = 0; i < vezes; i++) {
    digitalWrite(ledPin, HIGH);
    delay(300);
    digitalWrite(ledPin, LOW);
    delay(300);
  }
}
int randomMotivacao() {
  return random(0, sizeof(motivacao) / sizeof(motivacao[0]));
}
// === Sequência via WebSocket ===
void gerarSequenciaWS(String usr = "") {
  if (usr.length() > 0) user = usr; // salva usuário que solicitou
  generatedSequence = "";
  for (int i = 0; i < 4; i++) {
    generatedSequence += String(random(0, 10));
  }
  timestampSequence = millis();
  countdownStart = millis();
  countdownValue = 60;
  countdownActive = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Seq: " + generatedSequence);
  lcd.setCursor(0, 1);
  lcd.print("Tempo: 60s");

  webSocket.sendTXT("sequencia:" + generatedSequence);
}

void cancelarSequenciaWS() {
  generatedSequence = "";
  countdownActive = false;
  brewActive = false;
  user = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Porqueeee?");
  lcd.setCursor(0, 1);
  lcd.print("Pra que cancelar?");
  webSocket.sendTXT("cancelada");
  delay(2000);
  lcd.clear();
  lcd.noBacklight();
  exibirCarrosselRolagem(motivacao[randomMotivacao()], 1000, 300, true);
}

// === Função Carrossel para Display ===
void exibirCarrossel(String mensagens[], int numMensagens, int tempoExibicao) {
  lcd.clear();
  
  for (int i = 0; i < numMensagens; i++) {
    String texto = mensagens[i];
    
    // Se o texto for maior que 16 caracteres, faz rolagem horizontal
    if (texto.length() > 16) {
      // Exibe o texto completo com rolagem
      for (int pos = 0; pos <= texto.length() - 16; pos++) {
        lcd.clear();
        lcd.setCursor(0, 0);
        String parte = texto.substring(pos, pos + 16);
        lcd.print(parte);
        delay(300); // Velocidade da rolagem
      }
      delay(tempoExibicao); // Pausa no final
    } else {
      // Texto cabe na tela, exibe normalmente
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(texto);
      delay(tempoExibicao);
    }
  }
  lcd.noBacklight();
}

// === Função Carrossel com rolagem em duas linhas ===
void exibirCarrosselDuasLinhas(String mensagens[], int numMensagens, int tempoExibicao) {
  lcd.clear();
  
  for (int i = 0; i < numMensagens; i++) {
    String texto = mensagens[i];
    
    // Se o texto for maior que 32 caracteres (16x2), faz rolagem
    if (texto.length() > 32) {
      // Exibe o texto completo com rolagem em duas linhas
      for (int pos = 0; pos <= texto.length() - 32; pos++) {
        lcd.clear();
        lcd.setCursor(0, 0);
        String linha1 = texto.substring(pos, pos + 16);
        lcd.print(linha1);
        lcd.setCursor(0, 1);
        String linha2 = texto.substring(pos + 16, pos + 32);
        lcd.print(linha2);
        delay(300); // Velocidade da rolagem
      }
      delay(tempoExibicao); // Pausa no final
    } else if (texto.length() > 16) {
      // Texto cabe em duas linhas
      lcd.clear();
      lcd.setCursor(0, 0);
      String linha1 = texto.substring(0, 16);
      lcd.print(linha1);
      lcd.setCursor(0, 1);
      String linha2 = texto.substring(16);
      lcd.print(linha2);
      delay(tempoExibicao);
    } else {
      // Texto cabe em uma linha
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(texto);
      delay(tempoExibicao);
    }
  }
  
  lcd.clear();
  lcd.print("Cafezao da Computacao");
  lcd.noBacklight();
}

// === Função Carrossel com rolagem contínua ===
void exibirCarrosselRolagem(String texto, int tempoExibicao, int velocidadeRolagem, bool desligarBacklight) {
  lcd.clear();
  
  if (texto.length() <= 16) {
    // Texto cabe na tela
    lcd.setCursor(0, 0);
    lcd.print(texto);
    delay(tempoExibicao);
  } else {
    // Texto maior que 16 caracteres - rolagem contínua
    for (int pos = 0; pos <= texto.length() - 16; pos++) {
      lcd.clear();
      lcd.setCursor(0, 0);
      String parte = texto.substring(pos, pos + 16);
      lcd.print(parte);
      delay(velocidadeRolagem);
    }
    delay(tempoExibicao); // Pausa no final
  }
  
  if (desligarBacklight) lcd.noBacklight();
}

// === Rolagem NÃO-BLOQUEANTE (scrollActive) ===
void iniciarScroll(String txt) {
  scrollText = txt + "                "; // 16 espaços para dar tempo de ler
  scrollPos = 0;
  scrollActive = true;
  scrollLast = millis();
  lcd.setCursor(0, 0);
  lcd.print(scrollText.substring(0, displayWidth));
}

void atualizarScroll() {
  if (!scrollActive) return;
  if (millis() - scrollLast < scrollSpeed) return;
  scrollLast = millis();
  scrollPos++;
  if (scrollPos >= scrollText.length() - displayWidth) {
    scrollPos = 0;
  }
  lcd.setCursor(0, 0);
  lcd.print(scrollText.substring(scrollPos, scrollPos + displayWidth));
}

// === Função Carrossel com posição personalizada ===
void exibirCarrosselPosicao(String mensagens[], int numMensagens, int linha, int coluna, int tempoExibicao = 3000) {
  lcd.backlight();
  
  for (int i = 0; i < numMensagens; i++) {
    lcd.setCursor(coluna, linha);
    lcd.print("                "); // Limpa a linha
    lcd.setCursor(coluna, linha);
    
    // Trunca se for muito longa
    String msg = mensagens[i];
    if (msg.length() > 16 - coluna) {
      msg = msg.substring(0, 16 - coluna);
    }
    
    lcd.print(msg);
    delay(tempoExibicao);
  }
}

// === Função Carrossel simples (uma linha) ===
void exibirCarrosselSimples(String mensagens[], int numMensagens, int linha = 0, int tempoExibicao = 2000) {
  lcd.backlight();
  
  for (int i = 0; i < numMensagens; i++) {
    lcd.setCursor(0, linha);
    lcd.print("                "); // Limpa a linha
    lcd.setCursor(0, linha);
    
    String msg = mensagens[i];
    if (msg.length() > 16) {
      msg = msg.substring(0, 16);
    }
    
    lcd.print(msg);
    delay(tempoExibicao);
  }
}

void validarSequenciaWS(String seq, String quantity) {
  if (generatedSequence == "") {
    webSocket.sendTXT("erro:nenhuma_sequencia");
    return;
  }
  if (millis() - timestampSequence > 60000) {
    generatedSequence = "";
    user = "";
    countdownActive = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tempo Expirado");
    delay(2000);
    lcd.noBacklight();
    webSocket.sendTXT("erro:expirada");
    return;
  }
  if (seq == generatedSequence) {
    countdownActive = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Valida!");
    lcd.setCursor(0, 1);
    if (quantity == "1/4") {
      lcd.print("Copo 1/4");
      servoHalf.write(160);
      delay(500);
      servoHalf.write(130);
    } else if (quantity == "2/4") {
      lcd.print("Copo 2/4");
      servoQuarter.write(150);
      delay(500);
      servoQuarter.write(180);
    } else {
      lcd.print("Qtd invalida");
      delay(2000);
      lcd.noBacklight();
      webSocket.sendTXT("erro:qtd_invalida");
      return;
    }
    webSocket.sendTXT("ok:validada");

    // === inicia cooldown de preparo ===
    brewActive = true;
    brewStart = millis();
    brewPrevRemain = 61;
    brewRandomShown = false;
    brewMessage = motivacao[randomMotivacao()];
    lcd.clear();
    lcd.backlight();
    lcd.setCursor(0,0);
    iniciarScroll("Calma, ja vai ficar pronto!");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Invalida!");
    delay(2000);
    lcd.noBacklight();
    webSocket.sendTXT("erro:seq_invalida");
  }
}

// Gera string aleatória alfanumérica
String gerarRandom(int len) {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  String res;
  for (int i = 0; i < len; i++) {
    res += charset[random(sizeof(charset) - 1)];
  }
  return res;
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Desconectado do servidor WebSocket");
      break;
    case WStype_CONNECTED:
      Serial.println("Conectado ao servidor WebSocket");
      {
        String identificador = ssid + "|" + password + "|" + emailLogin + "|" + randomId;
        webSocket.sendTXT(identificador);
      }
      break;
    case WStype_TEXT:
      Serial.printf("Comando recebido: %s\n", payload);

      String cmd = String((char*)payload);
      Serial.println(cmd);
      if (cmd == "piscar") {
        piscarLed(3);
        webSocket.sendTXT("LED piscou 3 vezes.");
      } else if (cmd == "limpar") {
        webSocket.sendTXT("Credenciais Wi-Fi apagadas. Reiniciando...");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Apagando credenciais...");
        lcd.setCursor(0, 1);
        lcd.print("Reiniciando...");
        lcd.noBacklight();
        delay(2000);
        preferences.remove("ssid");
        preferences.remove("password");
        preferences.remove("email");
        preferences.end();
        WiFi.disconnect(true, true);
        delay(1000);
        ESP.restart();
      } else if (cmd.startsWith("gerar|") || cmd.startsWith("generate|")) {
          int p1 = cmd.indexOf('|');
          String newUser = cmd.substring(p1 + 1);
          if (newUser == user && generatedSequence.length() > 0 && countdownActive) {
            timestampSequence = millis();
            countdownStart = millis();
            lcd.backlight();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Seq: " + generatedSequence);
            lcd.setCursor(0,1);
            lcd.print("Tempo: 60s");
            webSocket.sendTXT("sequencia:" + generatedSequence);
          } else {
            if (isBusy()) {
              webSocket.sendTXT("erro:ocupado");
            } else {
              lcd.backlight();
              gerarSequenciaWS(newUser);
            }
          }
      } else if (cmd.startsWith("validar|") || cmd.startsWith("validate|")) {
        Serial.println("validar|" + cmd);
          int p1 = cmd.indexOf('|');
          int p2 = cmd.indexOf('|', p1 + 1);
          if (p1 > 0 && p2 > p1) {
            String seq = cmd.substring(p1 + 1, p2);
            String qty = cmd.substring(p2 + 1);
            Serial.println("validar|" + seq + "|" + qty);
            
            validarSequenciaWS(seq, qty);
          } else {
            webSocket.sendTXT("erro:formato");
          }
      }else if (cmd == "cancelar"){
        cancelarSequenciaWS();
      } else if (cmd.startsWith("Registrado como ")) {
        String nome = cmd.substring(16);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WI-FI:"+ ssid);
        lcd.setCursor(0, 1);
        lcd.print(nome);
        delay(2000);
        lcd.clear();
        exibirCarrosselRolagem("Cafezao da Computacao", 2000, 300, true);
        lcd.noBacklight();
      } else {
        lcd.clear();
        lcd.backlight();
        lcd.setCursor(0, 0);
        lcd.print("Cmd desconhecido");
        lcd.setCursor(0, 1);
        lcd.print(cmd);
        webSocket.sendTXT("erro:comando_invalido");
        delay(2000);
        lcd.clear();
        lcd.noBacklight();
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

// === NOVAS ROTAS PARA SEQUÊNCIA ===
void handleGenerateSequence() {
  if (server.method() != HTTP_GET) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  if (isBusy()) {
    server.send(423, "text/plain", "ocupado");
    return;
  }
  generatedSequence = "";
  for (int i = 0; i < 4; i++) {
    generatedSequence += String(random(0, 10));
  }
  timestampSequence = millis();
  countdownStart = millis();
  countdownValue = 60;
  countdownActive = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Seq: " + generatedSequence);
  lcd.setCursor(0, 1);
  lcd.print("Tempo: 60s");

  DynamicJsonDocument doc(256);
  doc["sequence"] = generatedSequence;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleValidateSequence() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  if (generatedSequence == "") {
    server.send(400, "application/json", "{\"detail\": \"No sequence generated yet.\"}");
    return;
  }
  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, body)) {
    server.send(400, "application/json", "{\"detail\": \"Invalid JSON.\"}");
    return;
  }
  String inputSequence = doc["sequence"] | "";
  String quantity = doc["quantity"] | "";
  if (millis() - timestampSequence > 60000) {
    generatedSequence = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tempo Expirado");
    server.send(400, "application/json", "{\"detail\": \"Time expired. Generate a new sequence.\"}");
    return;
  }
  if (inputSequence == generatedSequence) {
    countdownActive = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Valida!");
    lcd.setCursor(0, 1);
    if (quantity == "1/4") {
      lcd.print("Copo de 1/4");
      servoHalf.write(160);
      delay(500);
      servoHalf.write(130);
    } else if (quantity == "2/4") {
      lcd.print("Copo de 2/4");
      servoQuarter.write(150);
      delay(500);
      servoQuarter.write(180);
    } else {
      lcd.print("Qtd invalida!");
    }
    generatedSequence = "";
    server.send(200, "application/json", "{\"message\": \"Sequence validated successfully!\"}");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Invalida!");
    server.send(400, "application/json", "{\"detail\": \"Invalid sequence.\"}");
  }
}

void startConfigPortal() {
  IPAddress local_ip(192, 168, 10, 1);     // IP do AP
  IPAddress gateway(192, 168, 10, 1);      // Gateway padrão
  IPAddress subnet(255, 255, 255, 0);      // Máscara de sub-rede

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet); // Define IP personalizado
  WiFi.softAP("Café automatico");              // Nome da rede Wi-Fi

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
  Serial.begin(115200);
  randomSeed(esp_random());

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  servoHalf.attach(servoHalfPin);
  servoQuarter.attach(servoQuarterPin);
  servoHalf.write(130);
  servoQuarter.write(180);

  preferences.begin("credenciais", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  emailLogin = preferences.getString("email", "");

  randomId = preferences.getString("rand", "");
  if (randomId.length() == 0) {
    randomId = gerarRandom(5);
    preferences.putString("rand", randomId);
  }

  if (ssid.length() == 0) {
    startConfigPortal();
    return;
  }

  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFalha ao conectar, iniciando portal de configuração.");
    startConfigPortal();
    return;
  }

  Serial.println("\nWiFi conectado!");

  // Configurações do WebSocket
  webSocket.setReconnectInterval(5000);
  webSocket.setAuthorization("esp", "neurelix");
  webSocket.setExtraHeaders("Sec-WebSocket-Extensions:");
  webSocket.onEvent(webSocketEvent);
  webSocket.beginSSL("esp-conecta.neurelix.com.br", 443, "/");

  server.begin();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();

    if (millis() - lastPing > pingInterval) {
      webSocket.sendTXT("ping");
      Serial.println("ping");
      lastPing = millis();
    }
    server.handleClient(); // processa HTTP mesmo conectado
  } else {
    dnsServer.processNextRequest();
    server.handleClient();
  }
  // Atualiza contagem regressiva, se ativa
  if (countdownActive) {
    unsigned long elapsed = (millis() - countdownStart) / 1000;
    int remaining = 60 - elapsed;
    if (remaining != countdownValue && remaining >= 0) {
      countdownValue = remaining;
      lcd.setCursor(7, 1);
      lcd.print("    ");
      lcd.setCursor(7, 1);
      lcd.print(String(remaining) + "s");
    }
    if (remaining <= 0) {
      countdownActive = false;
      generatedSequence = "";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Tempo Expirado");
    }
  }
  // Atualiza contagem regressiva do preparo
  if (brewActive) {
    unsigned long elapsed = (millis() - brewStart) / 1000;
    int remaining = 60 - elapsed;
    if (remaining != brewPrevRemain && remaining >= 0) {
      brewPrevRemain = remaining;
      lcd.setCursor(0,1);
      lcd.print("Tempo: ");
      lcd.setCursor(7,1);
      lcd.print("   ");
      lcd.setCursor(7,1);
      lcd.print(String(remaining) + "s");
    }
    if (elapsed >= 10 && !brewRandomShown) {
      lcd.clear();
      lcd.backlight();
      lcd.setCursor(0,0);
      String msg = brewMessage;
      iniciarScroll(msg);
      brewRandomShown = true;
    }
    if (remaining <= 0) {
      brewActive = false;
      scrollActive = false;
      lcd.clear();
      lcd.backlight();
      lcd.print("Cafe pronto!");
      user = "";  
      generatedSequence = "";
      delay(3000);
      exibirCarrosselRolagem("Cafezao da Computacao", 100, 300, true);
      lcd.noBacklight();
    }
  }

  // Atualiza scroll não-bloqueante
  atualizarScroll();
}
