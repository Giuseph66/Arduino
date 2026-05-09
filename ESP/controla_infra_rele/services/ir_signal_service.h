#pragma once

String getProtocolName(const decode_results& decoded) {
  String protocol = typeToString(decoded.decode_type);
  protocol.toUpperCase();
  if (protocol.length() == 0) return "UNKNOWN";
  return protocol;
}

void copyRawData(const decode_results& decoded, IRCommand& command) {
  command.rawLength = 0;
  if (decoded.rawlen <= 1) return;
  uint16_t length = min<uint16_t>(decoded.rawlen - 1, MAX_RAW_LEN);
  for (uint16_t i = 0; i < length; i++) {
    uint32_t microsValue = decoded.rawbuf[i + 1] * kRawTick;
    command.rawData[i] = microsValue > 65535 ? 65535 : microsValue;
  }
  command.rawLength = length;
}

String commandToJson(const IRCommand& command, bool includeRawPreview) {
  String json = "{";
  json += "\"id\":\"" + jsonEscape(command.id) + "\",";
  json += "\"device\":\"" + jsonEscape(command.device) + "\",";
  json += "\"name\":\"" + jsonEscape(command.name) + "\",";
  json += "\"protocol\":\"" + jsonEscape(command.protocol) + "\",";
  json += "\"value\":\"" + uint64ToHex(command.value) + "\",";
  json += "\"bits\":" + String(command.bits) + ",";
  json += "\"rawLength\":" + String(command.rawLength) + ",";
  json += "\"frequency\":" + String(command.frequency) + ",";
  json += "\"timestamp\":" + String(command.timestamp);
  if (includeRawPreview) {
    uint16_t previewLength = min<uint16_t>(command.rawLength, 30);
    json += ",\"rawPreview\":[";
    for (uint16_t i = 0; i < previewLength; i++) {
      if (i) json += ",";
      json += String(command.rawData[i]);
    }
    json += "]";
  }
  json += "}";
  return json;
}

String lastSignalToJson() {
  if (!hasLastSignal) return "{\"hasSignal\":false}";
  String json = commandToJson(lastSignal, true);
  json.remove(json.length() - 1);
  json += ",\"hasSignal\":true}";
  return json;
}

String commandsToJson() {
  String json = "[";
  for (uint8_t i = 0; i < commandsCount; i++) {
    if (i) json += ",";
    json += commandToJson(commands[i], false);
  }
  json += "]";
  return json;
}

String rawDataToCsv(const IRCommand& command) {
  String csv;
  for (uint16_t i = 0; i < command.rawLength; i++) {
    if (i) csv += ",";
    csv += String(command.rawData[i]);
  }
  return csv;
}

String commandToExportJson(const IRCommand& command) {
  String json = commandToJson(command, false);
  json.remove(json.length() - 1);
  json += ",\"rawData\":\"" + rawDataToCsv(command) + "\"}";
  return json;
}

String exportCommandsToJson() {
  String json = "{\"version\":1,\"commands\":[";
  for (uint8_t i = 0; i < commandsCount; i++) {
    if (i) json += ",";
    json += commandToExportJson(commands[i]);
  }
  json += "]}";
  return json;
}

int findCommandIndex(const String& id) {
  for (uint8_t i = 0; i < commandsCount; i++) {
    if (commands[i].id == id) return i;
  }
  return -1;
}

bool sendRawFallback(const IRCommand& command) {
  if (command.rawLength == 0) return false;
  irsend.sendRaw(command.rawData, command.rawLength, command.frequency ? command.frequency : 38);
  return true;
}

bool sendIRSignal(const IRCommand& command) {
  if (command.rawLength == 0 && command.bits == 0) return false;
  receivePausedUntil = millis() + 3500;
  String protocol = command.protocol;
  protocol.toUpperCase();
  if (protocol == "NEC" && command.bits) irsend.sendNEC(command.value, command.bits);
  else if (protocol == "SONY" && command.bits) irsend.sendSony(command.value, command.bits);
  else if (protocol == "SAMSUNG" && command.bits) irsend.sendSAMSUNG(command.value, command.bits);
  else if (protocol == "LG" && command.bits) irsend.sendLG(command.value, command.bits);
  else if (protocol == "PANASONIC" && command.bits) irsend.sendPanasonic64(command.value, command.bits);
  else if (protocol == "JVC" && command.bits) irsend.sendJVC(command.value, command.bits, 0);
  else if (protocol == "RC5" && command.bits) irsend.sendRC5(command.value, command.bits);
  else if (protocol == "RC6" && command.bits) irsend.sendRC6(command.value, command.bits);
  else return sendRawFallback(command);
  return true;
}

void captureIR() {
  if (millis() < receivePausedUntil) {
    if (irrecv.decode(&results)) irrecv.resume();
    return;
  }
  if (!irrecv.decode(&results)) return;
  IRCommand command;
  command.id = "";
  command.device = "";
  command.name = "";
  command.protocol = getProtocolName(results);
  command.value = results.value;
  command.bits = results.bits;
  command.frequency = 38;
  command.timestamp = millis();
  if (command.protocol == "UNKNOWN") {
    command.value = 0;
    command.bits = 0;
  }
  copyRawData(results, command);
  lastSignal = command;
  hasLastSignal = true;

  Serial.println("Sinal IR capturado");
  Serial.print("Protocolo: ");
  Serial.println(lastSignal.protocol);
  Serial.print("Valor: ");
  Serial.println(uint64ToHex(lastSignal.value));
  Serial.print("Bits: ");
  Serial.println(lastSignal.bits);
  Serial.print("RAW length: ");
  Serial.println(lastSignal.rawLength);

  irrecv.resume();
}

