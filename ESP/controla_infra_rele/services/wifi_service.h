#pragma once

void connectWiFi() {
  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("ESP32 conectado ao Wi-Fi");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Acesse: http://");
  Serial.println(WiFi.localIP());
}

