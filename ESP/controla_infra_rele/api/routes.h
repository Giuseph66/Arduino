#pragma once

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/last", HTTP_GET, handleLastSignal);
  server.on("/api/commands", HTTP_GET, handleCommands);
  server.on("/api/export", HTTP_GET, handleExportCommands);
  server.on("/api/debug", HTTP_GET, handleDebug);
  server.on("/api/ir/catalog/device-types", HTTP_GET, handleCatalogDeviceTypes);
  server.on("/api/ir/catalog/brands", HTTP_GET, handleCatalogBrands);
  server.on("/api/ir/catalog/protocols", HTTP_GET, handleCatalogProtocols);
  server.on("/api/ir/catalog/candidates", HTTP_GET, handleCatalogCandidates);
  server.on("/api/ir/test-sessions", HTTP_GET, handleGetTestSession);
  server.on("/api/ir/test-sessions", HTTP_POST, handleCreateTestSession);
  server.on("/api/ir/test-sessions/send-next", HTTP_POST, handleSendNextCandidate);
  server.on("/api/ir/test-sessions/auto-start", HTTP_POST, handleAutoStart);
  server.on("/api/ir/test-sessions/auto-stop", HTTP_POST, handleAutoStop);
  server.on("/api/ir/test-sessions/rollback", HTTP_GET, handleRollback);
  server.on("/api/ir/test-sessions/confirm", HTTP_POST, handleConfirmCandidate);
  server.on("/api/ir/devices", HTTP_GET, handleCommands);
  server.on("/api/ir/devices/send-command", HTTP_POST, handleSendCommand);
  server.on("/api/ir/commands/external", HTTP_POST, handleImportCommand);
  server.on("/api/ir/learn/start", HTTP_POST, handleLearnStart);
  server.on("/api/ir/learn/save", HTTP_POST, handleSaveCommand);
  server.on("/api/save", HTTP_POST, handleSaveCommand);
  server.on("/api/import-command", HTTP_POST, handleImportCommand);
  server.on("/api/send-last", HTTP_POST, handleSendLast);
  server.on("/api/send", HTTP_POST, handleSendCommand);
  server.on("/api/delete", HTTP_POST, handleDeleteCommand);
  server.on("/api/clear", HTTP_POST, handleClearCommands);
  server.begin();
  Serial.println("Servidor web iniciado na porta 80");
}

