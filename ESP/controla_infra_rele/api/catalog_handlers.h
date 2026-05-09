#pragma once

void handleCatalogDeviceTypes() {
  String json = "[";
  uint8_t total = sizeof(catalogDeviceTypes) / sizeof(catalogDeviceTypes[0]);
  for (uint8_t i = 0; i < total; i++) {
    if (i) json += ",";
    json += "{\"id\":\"" + String(catalogDeviceTypes[i].id) + "\",\"label\":\"" + String(catalogDeviceTypes[i].label) + "\",\"state_based\":" + String(catalogDeviceTypes[i].stateBased ? "true" : "false") + "}";
  }
  json += "]";
  sendJson(200, json);
}

void handleCatalogBrands() {
  String deviceType = server.arg("deviceType");
  String json = "[";
  uint8_t count = 0;
  uint8_t total = sizeof(catalogBrands) / sizeof(catalogBrands[0]);
  for (uint8_t i = 0; i < total; i++) {
    if (deviceType.length() && !brandSupportsType(catalogBrands[i], deviceType)) continue;
    if (count++) json += ",";
    json += "{\"id\":\"" + String(catalogBrands[i].id) + "\",\"label\":\"" + String(catalogBrands[i].label) + "\"}";
  }
  json += "]";
  sendJson(200, json);
}

void handleCatalogProtocols() {
  sendJson(200, "[\"NEC\",\"SAMSUNG\",\"SONY\",\"LG\",\"PANASONIC\",\"JVC\",\"RC5\",\"RC6\",\"RAW\",\"GREE_AC\",\"DAIKIN_AC\",\"MIDEA_AC\",\"FUJITSU_AC\",\"SAMSUNG_AC\"]");
}

void handleCatalogCandidates() {
  String deviceType = server.arg("deviceType");
  String brand = server.arg("brand");
  String json = "[";
  uint8_t count = 0;
  uint16_t total = sizeof(catalogCandidates) / sizeof(catalogCandidates[0]);
  for (uint16_t i = 0; i < total; i++) {
    if (!candidateMatches(i, deviceType, brand)) continue;
    if (count++) json += ",";
    json += catalogCandidateToJson(i);
  }
  json += "]";
  sendJson(200, json);
}

void handleCreateTestSession() {
  String deviceType = getBodyValue("deviceType");
  String brand = getBodyValue("brand");
  if (deviceType.length() == 0) deviceType = "tv";
  testSession.active = true;
  testSession.autoMode = false;
  testSession.id = "sess_" + String(nextSessionId++);
  testSession.deviceType = deviceType;
  testSession.brand = brand;
  testSession.cursor = 0;
  testSession.sentCount = 0;
  uint16_t requestedDelay = getBodyValue("delayMs").toInt();
  testSession.delayMs = requestedDelay ? constrain(requestedDelay, 300, 5000) : 1500;
  testSession.nextSendAt = 0;
  testSession.eventCount = 0;
  sendJson(200, "{\"success\":true,\"id\":\"" + testSession.id + "\"}");
}

void handleGetTestSession() {
  String json = "{\"active\":" + String(testSession.active ? "true" : "false");
  json += ",\"autoMode\":" + String(testSession.autoMode ? "true" : "false");
  json += ",\"id\":\"" + testSession.id + "\",\"deviceType\":\"" + testSession.deviceType + "\",\"brand\":\"" + testSession.brand + "\",\"sentCount\":" + String(testSession.sentCount) + "}";
  sendJson(200, json);
}

void handleSendNextCandidate() {
  if (!testSession.active) {
    sendJson(400, "{\"success\":false,\"message\":\"Sessao de teste nao iniciada\"}");
    return;
  }
  int index = findNextCandidateIndex(testSession.deviceType, testSession.brand, testSession.cursor);
  if (index < 0) {
    sendJson(404, "{\"success\":false,\"message\":\"Fim da fila de candidatos\"}");
    return;
  }
  testSession.cursor = index + 1;
  testSession.sentCount++;
  bool ok = sendCatalogCandidate(index);
  String json = "{\"success\":" + String(ok ? "true" : "false") + ",\"message\":\"" + String(ok ? "Candidato enviado" : "Candidato nao enviado nesta versao") + "\",\"candidate\":";
  json += catalogCandidateToJson(index);
  json += "}";
  sendJson(ok ? 200 : 400, json);
}

void handleAutoStart() {
  if (!testSession.active) {
    sendJson(400, "{\"success\":false,\"message\":\"Sessao de teste nao iniciada\"}");
    return;
  }
  uint16_t requestedDelay = getBodyValue("delayMs").toInt();
  testSession.delayMs = requestedDelay ? constrain(requestedDelay, 300, 5000) : 1500;
  testSession.autoMode = true;
  testSession.nextSendAt = millis();
  sendJson(200, "{\"success\":true,\"message\":\"Scan automatico iniciado\"}");
}

void handleAutoStop() {
  testSession.autoMode = false;
  sendJson(200, "{\"success\":true,\"message\":\"Scan parado\"}");
}

void handleRollback() {
  String json = "[";
  for (uint8_t i = 0; i < testSession.eventCount; i++) {
    if (i) json += ",";
    json += catalogCandidateToJson(testSession.events[i].candidateIndex);
    json.remove(json.length() - 1);
    json += ",\"sentAt\":" + String(testSession.events[i].sentAt) + "}";
  }
  json += "]";
  sendJson(200, json);
}

void handleConfirmCandidate() {
  int index = getBodyValue("candidateIndex").toInt();
  if (index < 0 || index >= (int)(sizeof(catalogCandidates) / sizeof(catalogCandidates[0]))) {
    sendJson(400, "{\"success\":false,\"message\":\"Candidato invalido\"}");
    return;
  }
  if (commandsCount >= MAX_COMMANDS) {
    sendJson(400, "{\"success\":false,\"message\":\"Limite de comandos atingido\"}");
    return;
  }
  const CatalogCandidate& c = catalogCandidates[index];
  if (String(c.type) == "climate_state") {
    sendJson(400, "{\"success\":false,\"message\":\"Perfil de ar-condicionado identificado, mas gerador AC nativo ainda nao salvo como comando simples\"}");
    return;
  }
  IRCommand command;
  command.id = "cmd_" + String(nextCommandId++);
  command.device = String(c.brand) + " " + String(c.model);
  command.name = c.label;
  command.protocol = c.protocol;
  command.value = c.value;
  command.bits = c.bits;
  command.frequency = c.carrier;
  command.rawLength = 0;
  command.timestamp = millis();
  commands[commandsCount++] = command;
  testSession.autoMode = false;
  sendJson(200, "{\"success\":true,\"message\":\"Perfil salvo\",\"id\":\"" + command.id + "\"}");
}

