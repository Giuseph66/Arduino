#pragma once

void sendJson(uint16_t code, const String& json) {
  Serial.print("HTTP ");
  Serial.print(server.uri());
  Serial.print(" -> ");
  Serial.println(code);
  server.send(code, "application/json", json);
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  String json = "{";
  json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"commandsCount\":" + String(commandsCount) + ",";
  json += "\"hasLastSignal\":" + String(hasLastSignal ? "true" : "false") + ",";
  json += "\"uptime\":" + String(millis());
  json += "}";
  sendJson(200, json);
}

void handleDebug() {
  String text = "Central IR ESP32\n";
  text += "IP: " + WiFi.localIP().toString() + "\n";
  text += "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "OK" : "OFF") + "\n";
  text += "hasLastSignal: " + String(hasLastSignal ? "true" : "false") + "\n";
  text += "protocol: " + lastSignal.protocol + "\n";
  text += "value: " + uint64ToHex(lastSignal.value) + "\n";
  text += "bits: " + String(lastSignal.bits) + "\n";
  text += "rawLength: " + String(lastSignal.rawLength) + "\n";
  text += "commandsCount: " + String(commandsCount) + "\n";
  server.send(200, "text/plain", text);
}

