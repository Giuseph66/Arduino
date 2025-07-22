#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

String ssid;
String password;
String emailLogin;
Preferences preferences;
WebServer server(80);

String randomId;
const char* host = "esp-conecta.neurelix.com.br";
const int ledPin = 2;
unsigned long lastPing = 0;
const unsigned long pingInterval = 50000;

LiquidCrystal_I2C lcd(0x27, 16, 2);

WebSocketsClient webSocket;
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Função para piscar o LED
void piscarLed(int vezes) {
  for (int i = 0; i < vezes; i++) {
    digitalWrite(ledPin, HIGH);
    delay(300);
    digitalWrite(ledPin, LOW);
    delay(300);
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

// Função para interpretar ASCII art como pixels
void exibirAsciiArt(String asciiData) {
  lcd.clear();
  lcd.backlight();
  
  // Remove "ascii:" do início se presente
  if (asciiData.startsWith("ascii:")) {
    asciiData = asciiData.substring(6);
  }
  
  // Divide em linhas
  String linhas[8];
  int numLinhas = 0;
  int pos = 0;
  
  while (pos < asciiData.length() && numLinhas < 8) {
    int endPos = asciiData.indexOf(',', pos);
    if (endPos == -1) {
      endPos = asciiData.length();
    }
    
    String linha = asciiData.substring(pos, endPos);
    linha.trim(); // Remove espaços
    
    // Remove "B" do início se presente
    if (linha.startsWith("B")) {
      linha = linha.substring(1);
    }
    
    linhas[numLinhas] = linha;
    numLinhas++;
    
    pos = endPos + 1;
  }
  
  // Converte cada linha para pixels no LCD
  for (int i = 0; i < numLinhas && i < 2; i++) { // LCD tem apenas 2 linhas
    String linha = linhas[i];
    String linhaLCD = "";
    
    // Converte cada caractere da linha
    for (int j = 0; j < linha.length() && j < 16; j++) { // LCD tem 16 colunas
      char pixel = linha.charAt(j);
      if (pixel == '1') {
        linhaLCD += "█"; // Caractere preenchido
      } else {
        linhaLCD += " "; // Espaço vazio
      }
    }
    
    // Preenche o resto da linha com espaços
    while (linhaLCD.length() < 16) {
      linhaLCD += " ";
    }
    
    // Exibe no LCD
    lcd.setCursor(0, i);
    lcd.print(linhaLCD);
  }
  
  // Se há mais linhas, faz rolagem para mostrar
  if (numLinhas > 2) {
    delay(2000);
    for (int i = 2; i < numLinhas; i++) {
      String linha = linhas[i];
      String linhaLCD = "";
      
      for (int j = 0; j < linha.length() && j < 16; j++) {
        char pixel = linha.charAt(j);
        if (pixel == '1') {
          linhaLCD += "█";
        } else {
          linhaLCD += " ";
        }
      }
      
      while (linhaLCD.length() < 16) {
        linhaLCD += " ";
      }
      
      lcd.setCursor(0, 0);
      lcd.print(linhaLCD);
      
      if (i + 1 < numLinhas) {
        String linha2 = linhas[i + 1];
        String linhaLCD2 = "";
        
        for (int j = 0; j < linha2.length() && j < 16; j++) {
          char pixel = linha2.charAt(j);
          if (pixel == '1') {
            linhaLCD2 += "█";
          } else {
            linhaLCD2 += " ";
          }
        }
        
        while (linhaLCD2.length() < 16) {
          linhaLCD2 += " ";
        }
        
        lcd.setCursor(0, 1);
        lcd.print(linhaLCD2);
        i++; // Pula a próxima linha pois já foi exibida
      }
      
      delay(1000);
    }
  }
  
  delay(3000);
  lcd.clear();
  lcd.noBacklight();
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
      
      // Verifica se é um comando ASCII art
      if (cmd.startsWith("ascii:")) {
        exibirAsciiArt(cmd);
        webSocket.sendTXT("ASCII art exibido");
      } else {
        // Exibe o comando normal no LCD
        lcd.clear();
        lcd.backlight();
        lcd.setCursor(0, 0);
        lcd.print("Comando:");
        lcd.setCursor(0, 1);
        
        // Se o comando for muito longo, faz rolagem
        if (cmd.length() > 16) {
          for (int pos = 0; pos <= cmd.length() - 16; pos++) {
            lcd.setCursor(0, 1);
            lcd.print("                ");
            lcd.setCursor(0, 1);
            String parte = cmd.substring(pos, pos + 16);
            lcd.print(parte);
            delay(500);
          }
        } else {
          lcd.print(cmd);
        }
        
        // Confirma recebimento
        webSocket.sendTXT("Comando recebido: " + cmd);
        
        delay(2000);
        lcd.clear();
        lcd.noBacklight();
      }
      break;
  }
}

// Portal de configuração
void handleRoot() {
  int n = WiFi.scanNetworks();
  String options = "";
  for (int i = 0; i < n; i++) {
    String ssidItem = WiFi.SSID(i);
    if (ssidItem.length() == 0) continue;
    if (options.indexOf("value='" + ssidItem + "'") >= 0) continue;
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
  IPAddress local_ip(192, 168, 10, 1);
  IPAddress gateway(192, 168, 10, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP("ESP32 Config");

  Serial.println("Portal de configuração ativo. Conecte-se à rede: ESP32 Config");
  Serial.print("Acesse: http://");
  Serial.println(local_ip);

  dnsServer.start(DNS_PORT, "*", local_ip);
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

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
    server.handleClient();
  } else {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}
