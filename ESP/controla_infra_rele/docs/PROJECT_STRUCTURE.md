# Estrutura

- `controla_infra_rele.ino`: arquivo central do Arduino, contem apenas includes, `setup()` e `loop()`.
- `config/config.h`: pinos, limites e credenciais Wi-Fi.
- `core/app_state.h`: structs e variaveis globais do firmware.
- `core/utils.h`: helpers de JSON, body POST e hexadecimal.
- `web/ui_page.h`: pagina HTML/CSS/JS servida pelo ESP32.
- `catalog/catalog_seed.h`: seed embarcado do catalogo IR.
- `services/wifi_service.h`: conexao Wi-Fi.
- `services/ir_signal_service.h`: captura, envio, exportacao e armazenamento RAM dos comandos IR.
- `services/catalog_service.h`: regras de catalogo, sessoes de teste, rollback e scan automatico.
- `api/core_handlers.h`: handlers basicos HTTP/status/root/debug.
- `api/command_handlers.h`: handlers de comandos, import/export e clonagem.
- `api/catalog_handlers.h`: handlers do catalogo e identificacao IR.
- `api/routes.h`: registro das rotas do servidor.
- `ir_universal_catalog_seed.json`: base original de referencia.
