#include "esp_camera.h"
#include <WiFi.h>

// ======= CONFIGURAÇÃO DO MODELO DE CÂMERA =======
// AI‑Thinker pinout:
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ======= CREDENCIAIS WiFi =======
const char* ssid     = "COMPANIA";
const char* password = "jesusateu123";

WiFiServer server(80);

// ======= FUNÇÃO PARA INICIALIZAR A CÂMERA =======
void initCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Framesize & qualidade
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 2; // dois framebuffers para streaming

  // Inicia
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Erro ao iniciar a câmera: 0x%x\n", err);
    while(true);
  }
}

// ======= FUNÇÕES AUXILIARES =======
void handleCapture() {
  WiFiClient client = server.available();
  if (!client) return;

  String request = client.readStringUntil('\r');
  client.flush();

  if (request.indexOf("/capture") != -1) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      client.println("HTTP/1.1 500 Internal Server Error");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("Falha ao capturar imagem");
    } else {
      client.println("HTTP/1.1 200 OK");
      client.printf("Content-Type: image/jpeg\r\n");
      client.printf("Content-Length: %u\r\n", fb->len);
      client.println();
      client.write(fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
  }
  else if (request.indexOf("/stream") != -1) {
    // Envia fluxo MJPEG
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println("Cache-Control: no-cache");
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      camera_fb_t * fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Falha ao capturar frame");
        continue;
      }
      client.print("--frame\r\n");
      client.print("Content-Type: image/jpeg\r\n");
      client.printf("Content-Length: %u\r\n\r\n", fb->len);
      client.write(fb->buf, fb->len);
      client.print("\r\n");
      esp_camera_fb_return(fb);

      // Limita FPS (~20fps)
      delay(50);
    }
  }
  else if (request.indexOf("/ledOn") != -1) {
    // Liga o LED de flash (SCCB pin)
    digitalWrite(4, HIGH);
    client.println("HTTP/1.1 200 OK\r\r\nLED ON");
  }
  else if (request.indexOf("/ledOff") != -1) {
    digitalWrite(4, LOW);
    client.println("HTTP/1.1 200 OK\r\n\r\nLED OFF");
  }
  else {
    // Página principal: fornece link para stream e capture
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<html><head><title>ESP32-CAM Live</title><style>body{font-family:Arial;text-align:center;background:#111;color:#eee;} img{width:90%;max-width:640px;border:2px solid #444;border-radius:4px;margin-top:10px;} a{color:#0bf;text-decoration:none;margin:0 5px;}</style></head><body>");
    client.println("<h1>ESP32-CAM Live View</h1>");
    client.print("<img src=\"http://");
    client.print(WiFi.localIP());
    client.println("/stream\" alt=\"stream\" id=\"cam\">\n");
    client.println("<p><a href=\"/capture\">Tirar Foto</a></p>");
    client.println("<p><a href=\"/ledOn\">LED ON</a> | <a href=\"/ledOff\">LED OFF</a></p>");
    client.println("</body></html>");
  }
  delay(1);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  pinMode(4, OUTPUT);      // LED flash no pino GPIO4
  digitalWrite(4, LOW);    // LED desligado

  initCamera();

  // Conecta no WiFi
  Serial.printf("Conectando em %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConectado! IP: %s\n", WiFi.localIP().toString().c_str());

  server.begin();
}

void loop() {
  handleCapture();
}
