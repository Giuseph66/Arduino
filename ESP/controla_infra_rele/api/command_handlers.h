#pragma once

void handleLastSignal() {
  sendJson(200, lastSignalToJson());
}

void handleCommands() {
  sendJson(200, commandsToJson());
}

void handleExportCommands() {
  sendJson(200, exportCommandsToJson());
}

void handleSaveCommand() {
  if (!hasLastSignal) {
    sendJson(400, "{\"success\":false,\"message\":\"Nenhum sinal IR capturado ainda\"}");
    return;
  }
  if (commandsCount >= MAX_COMMANDS) {
    sendJson(400, "{\"success\":false,\"message\":\"Limite de comandos atingido\"}");
    return;
  }
  String device = getBodyValue("device");
  String name = getBodyValue("name");
  if (device.length() == 0) device = "Outro";
  if (name.length() == 0) name = "Comando";
  IRCommand command = lastSignal;
  command.id = "cmd_" + String(nextCommandId++);
  command.device = device;
  command.name = name;
  commands[commandsCount++] = command;
  sendJson(200, "{\"success\":true,\"message\":\"Comando salvo com sucesso\",\"id\":\"" + command.id + "\"}");
}

void importRawData(const String& csv, IRCommand& command) {
  command.rawLength = 0;
  int start = 0;
  while (start >= 0 && command.rawLength < MAX_RAW_LEN) {
    int comma = csv.indexOf(',', start);
    String item = comma >= 0 ? csv.substring(start, comma) : csv.substring(start);
    item.trim();
    if (item.length()) command.rawData[command.rawLength++] = (uint16_t)item.toInt();
    if (comma < 0) break;
    start = comma + 1;
  }
}

void handleImportCommand() {
  if (commandsCount >= MAX_COMMANDS) {
    sendJson(400, "{\"success\":false,\"message\":\"Limite de comandos atingido\"}");
    return;
  }
  IRCommand command;
  command.id = "cmd_" + String(nextCommandId++);
  command.device = getBodyValue("device");
  command.name = getBodyValue("name");
  command.protocol = getBodyValue("protocol");
  command.value = strtoull(getBodyValue("value").c_str(), nullptr, 0);
  command.bits = (uint16_t)getBodyValue("bits").toInt();
  command.frequency = (uint16_t)getBodyValue("frequency").toInt();
  command.timestamp = millis();
  if (command.device.length() == 0) command.device = "Importado";
  if (command.name.length() == 0) command.name = "Comando";
  if (command.protocol.length() == 0) command.protocol = "RAW";
  if (command.frequency == 0) command.frequency = 38;
  importRawData(getBodyValue("rawData"), command);
  if (command.rawLength == 0 && command.bits == 0) {
    sendJson(400, "{\"success\":false,\"message\":\"Comando importado sem dados IR\"}");
    return;
  }
  commands[commandsCount++] = command;
  sendJson(200, "{\"success\":true,\"message\":\"Comando importado\",\"id\":\"" + command.id + "\"}");
}

void handleSendLast() {
  if (!hasLastSignal) {
    sendJson(400, "{\"success\":false,\"message\":\"Nenhum sinal IR capturado ainda\"}");
    return;
  }
  bool ok = sendIRSignal(lastSignal);
  sendJson(ok ? 200 : 400, String("{\"success\":") + (ok ? "true" : "false") + ",\"message\":\"" + (ok ? "Ultimo sinal enviado" : "Erro ao enviar ultimo sinal") + "\"}");
}

void handleSendCommand() {
  String id = getBodyValue("id");
  int index = findCommandIndex(id);
  if (index < 0) {
    sendJson(404, "{\"success\":false,\"message\":\"Comando nao encontrado\"}");
    return;
  }
  bool ok = sendIRSignal(commands[index]);
  sendJson(ok ? 200 : 400, String("{\"success\":") + (ok ? "true" : "false") + ",\"message\":\"" + (ok ? "Comando enviado" : "Erro ao enviar comando") + "\"}");
}

void handleDeleteCommand() {
  String id = getBodyValue("id");
  int index = findCommandIndex(id);
  if (index < 0) {
    sendJson(404, "{\"success\":false,\"message\":\"Comando nao encontrado\"}");
    return;
  }
  for (uint8_t i = index; i + 1 < commandsCount; i++) {
    commands[i] = commands[i + 1];
  }
  commandsCount--;
  sendJson(200, "{\"success\":true,\"message\":\"Comando excluido\"}");
}

void handleClearCommands() {
  commandsCount = 0;
  sendJson(200, "{\"success\":true,\"message\":\"Comandos limpos\"}");
}

void handleLearnStart() {
  hasLastSignal = false;
  receivePausedUntil = 0;
  irrecv.resume();
  sendJson(200, "{\"success\":true,\"message\":\"Modo clonagem iniciado\"}");
}

