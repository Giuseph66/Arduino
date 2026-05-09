#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

#include "config/config.h"
#include "core/app_state.h"
#include "catalog/catalog_seed.h"
#include "web/ui_page.h"
#include "core/utils.h"
#include "services/ir_signal_service.h"
#include "services/catalog_service.h"
#include "services/wifi_service.h"
#include "api/core_handlers.h"
#include "api/command_handlers.h"
#include "api/catalog_handlers.h"
#include "api/routes.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  connectWiFi();
  irrecv.enableIRIn();
  irsend.begin();
  setupServer();
}

void loop() {
  server.handleClient();
  processCatalogAutoScan();
  captureIR();
}
