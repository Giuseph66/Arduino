#pragma once

bool brandSupportsType(const CatalogBrand& brand, const String& deviceType) {
  String types = "," + String(brand.deviceTypes) + ",";
  return types.indexOf("," + deviceType + ",") >= 0;
}

String catalogCandidateToJson(int index) {
  const CatalogCandidate& c = catalogCandidates[index];
  String json = "{";
  json += "\"id\":\"" + String(c.id) + "\",";
  json += "\"index\":" + String(index) + ",";
  json += "\"deviceType\":\"" + String(c.deviceType) + "\",";
  json += "\"brand\":\"" + String(c.brand) + "\",";
  json += "\"model\":\"" + String(c.model) + "\",";
  json += "\"commandKey\":\"" + String(c.commandKey) + "\",";
  json += "\"label\":\"" + String(c.label) + "\",";
  json += "\"type\":\"" + String(c.type) + "\",";
  json += "\"protocol\":\"" + String(c.protocol) + "\",";
  json += "\"code_hex\":\"" + uint64ToHex(c.value) + "\",";
  json += "\"bits\":" + String(c.bits) + ",";
  json += "\"carrier_khz\":" + String(c.carrier) + ",";
  json += "\"repeat\":" + String(c.repeat);
  json += "}";
  return json;
}

bool candidateMatches(int index, const String& deviceType, const String& brand) {
  const CatalogCandidate& c = catalogCandidates[index];
  return deviceType == c.deviceType && (brand.length() == 0 || brand == c.brand);
}

int findNextCandidateIndex(const String& deviceType, const String& brand, uint16_t start) {
  uint16_t total = sizeof(catalogCandidates) / sizeof(catalogCandidates[0]);
  for (uint16_t i = start; i < total; i++) {
    if (candidateMatches(i, deviceType, brand)) return i;
  }
  return -1;
}

void addTestEvent(int candidateIndex) {
  if (testSession.eventCount < 5) {
    testSession.events[testSession.eventCount++] = {candidateIndex, millis()};
    return;
  }
  for (uint8_t i = 1; i < 5; i++) testSession.events[i - 1] = testSession.events[i];
  testSession.events[4] = {candidateIndex, millis()};
}

bool sendCatalogCandidate(int index) {
  if (index < 0) return false;
  const CatalogCandidate& c = catalogCandidates[index];
  if (String(c.type) == "climate_state") {
    Serial.println("Climate state catalogado. Gerador AC nativo ainda nao embarcado nesta versao.");
    return false;
  }
  IRCommand command;
  command.protocol = c.protocol;
  command.value = c.value;
  command.bits = c.bits;
  command.frequency = c.carrier;
  command.rawLength = 0;
  bool ok = false;
  uint8_t repeat = c.repeat ? c.repeat : 1;
  for (uint8_t i = 0; i < repeat; i++) {
    ok = sendIRSignal(command);
    delay(45);
  }
  if (ok) addTestEvent(index);
  return ok;
}

void processCatalogAutoScan() {
  if (!testSession.active || !testSession.autoMode) return;
  if (millis() < testSession.nextSendAt) return;
  int index = findNextCandidateIndex(testSession.deviceType, testSession.brand, testSession.cursor);
  if (index < 0) {
    testSession.autoMode = false;
    return;
  }
  testSession.cursor = index + 1;
  testSession.sentCount++;
  sendCatalogCandidate(index);
  const CatalogCandidate& c = catalogCandidates[index];
  uint16_t delayMs = testSession.delayMs;
  if (String(c.commandKey).indexOf("power") >= 0 && delayMs < 2000) delayMs = 2000;
  testSession.nextSendAt = millis() + delayMs;
}

