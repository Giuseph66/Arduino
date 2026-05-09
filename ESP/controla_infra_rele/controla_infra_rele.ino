#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

#define IR_RECEIVE_PIN 15
#define IR_SEND_PIN 4
#define MAX_RAW_LEN 300
#define MAX_COMMANDS 50

const char* ssid = "COMPANIA";
const char* password = "jesusateu123";

struct IRCommand {
  String id;
  String device;
  String name;
  String protocol;
  uint64_t value;
  uint16_t bits;
  uint16_t rawData[MAX_RAW_LEN];
  uint16_t rawLength;
  uint16_t frequency;
  unsigned long timestamp;
};

WebServer server(80);
IRrecv irrecv(IR_RECEIVE_PIN);
IRsend irsend(IR_SEND_PIN);
decode_results results;

IRCommand lastSignal;
bool hasLastSignal = false;
IRCommand commands[MAX_COMMANDS];
uint8_t commandsCount = 0;
uint32_t nextCommandId = 1;
unsigned long receivePausedUntil = 0;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Central IR ESP32</title>
<style>
:root{color-scheme:dark;--bg:#07111f;--panel:rgba(12,24,39,.74);--panel2:rgba(255,255,255,.08);--line:rgba(255,255,255,.14);--text:#edf6ff;--muted:#9fb3c8;--ok:#35e58d;--bad:#ff637b;--brand:#62d8ff;--gold:#ffd166}
*{box-sizing:border-box}body{margin:0;font-family:Inter,system-ui,-apple-system,Segoe UI,sans-serif;background:radial-gradient(circle at 15% 0,#1d6b89 0,#07111f 32%,#03070d 100%);color:var(--text);min-height:100vh}button,input{font:inherit}.wrap{width:min(1200px,calc(100% - 28px));margin:auto;padding:24px 0 34px}
.top{display:flex;justify-content:space-between;gap:18px;align-items:flex-start;margin-bottom:18px}.title h1{margin:0;font-size:clamp(30px,6vw,54px);letter-spacing:0}.title p{margin:5px 0 0;color:var(--muted)}.tools{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}.pill,.badge{border:1px solid var(--line);background:rgba(255,255,255,.08);padding:7px 10px;border-radius:999px;color:var(--muted);font-size:13px}.pill strong{color:var(--text)}
.iconBtn{width:42px;height:42px;border-radius:14px;padding:0;display:grid;place-items:center;font-size:24px}.grid{display:grid;grid-template-columns:minmax(0,1fr) 360px;gap:16px}.card{border:1px solid var(--line);background:var(--panel);backdrop-filter:blur(18px);border-radius:18px;padding:18px;box-shadow:0 20px 60px rgba(0,0,0,.28)}.card h2{font-size:18px;margin:0 0 14px}.sub{color:var(--muted);font-size:13px}
.kv{display:grid;grid-template-columns:130px 1fr;gap:9px 12px;color:var(--muted)}.kv b{color:var(--text)}.raw{margin-top:12px;padding:12px;border-radius:12px;background:rgba(0,0,0,.24);font-family:ui-monospace,monospace;color:#c7f3ff;overflow:auto}.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}
button{border:0;border-radius:13px;padding:11px 14px;background:linear-gradient(135deg,#32d7ff,#558bff);color:#03101c;font-weight:800;cursor:pointer;transition:.18s transform,.18s opacity}button:hover{transform:translateY(-1px)}button.ghost{background:rgba(255,255,255,.1);color:var(--text);border:1px solid var(--line)}button.danger{background:linear-gradient(135deg,#ff637b,#ff9f6e);color:#190408}button:disabled{opacity:.5;cursor:not-allowed}
input,select{width:100%;padding:12px;border-radius:12px;border:1px solid var(--line);background:rgba(0,0,0,.25);color:var(--text);outline:none}select option{background:#0b1726;color:var(--text)}label{display:block;color:var(--muted);font-size:13px;margin:10px 0 6px}.equipments{display:grid;gap:12px}.device{border:1px solid var(--line);border-radius:16px;background:rgba(0,0,0,.16);overflow:hidden}.devHead{display:flex;justify-content:space-between;gap:10px;align-items:center;padding:14px 15px;background:rgba(255,255,255,.05)}.devHead h3{margin:0;font-size:17px}.devBody{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:10px;padding:12px}.cmd{border:1px solid var(--line);border-radius:14px;padding:12px;background:rgba(255,255,255,.06)}.cmd h4{margin:0 0 8px}.cmdMeta{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}.logs{height:220px;overflow:auto;display:flex;flex-direction:column;gap:8px}.log{padding:9px 10px;border-radius:10px;background:rgba(0,0,0,.22);color:var(--muted);font-size:13px}.ok{color:var(--ok)}.bad{color:var(--bad)}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.58);display:none;align-items:center;justify-content:center;padding:16px}.modal.open{display:flex}.dialog{width:min(460px,100%);border:1px solid var(--line);border-radius:18px;background:#0b1726;padding:18px;box-shadow:0 30px 90px rgba(0,0,0,.5)}.dialogHead{display:flex;align-items:center;justify-content:space-between}.dialogHead h2{margin:0}.toast{position:fixed;right:16px;bottom:16px;padding:13px 15px;border-radius:12px;background:#102033;border:1px solid var(--line);box-shadow:0 12px 40px rgba(0,0,0,.35);display:none}
@media(max-width:860px){.top{display:block}.tools{justify-content:flex-start;margin-top:14px}.grid{grid-template-columns:1fr}.kv{grid-template-columns:1fr}.wrap{width:min(100% - 18px,1200px);padding-top:18px}}
</style></head><body><main class="wrap">
<header class="top"><div class="title"><h1>Central IR ESP32</h1><p>Controle seus equipamentos por infravermelho</p></div><div class="tools"><span class="pill">Wi-Fi <strong id="wifi">...</strong></span><span class="pill">IP <strong id="ip">...</strong></span><span class="pill">Comandos <strong id="count">0</strong></span><span class="pill">Ultimo sinal <strong id="has">Nao</strong></span><button class="ghost" onclick="exportCommands()">Exportar</button><button class="ghost" onclick="$('importFile').click()">Importar</button><button class="iconBtn" onclick="openSave()">+</button><input id="importFile" type="file" accept="application/json" hidden onchange="importCommands(this.files[0])"></div></header>
<section class="grid"><div class="card"><h2>Ultimo sinal capturado</h2><div id="last" class="kv"></div><div id="raw" class="raw">Aguardando sinal IR...</div><div class="actions"><button onclick="sendLast()">Testar ultimo sinal</button><button onclick="openSave()">Salvar ultimo sinal</button><button class="ghost" onclick="loadLastSignal()">Atualizar</button></div><p class="sub">Ao testar, a recepcao IR pausa por alguns segundos para evitar recapturar o proprio envio.</p></div>
<div class="card"><h2>Logs</h2><div id="logs" class="logs"></div></div>
<div class="card" style="grid-column:1/-1"><div class="devHead"><div><h2>Equipamentos cadastrados</h2><span class="sub">Comandos agrupados por TV, ar, projetor ou outro aparelho.</span></div><button class="danger" onclick="clearCommands()">Limpar</button></div><div id="commands" class="equipments"></div></div></section></main>
<div id="modal" class="modal"><div class="dialog"><div class="dialogHead"><h2>Salvar comando</h2><button class="ghost iconBtn" onclick="closeSave()">x</button></div><label>Equipamento</label><select id="deviceSelect" onchange="toggleNewDevice()"><option value="TV">TV</option><option value="Ar-condicionado">Ar-condicionado</option><option value="Projetor">Projetor</option><option value="Som">Som</option><option value="Outro">Outro</option><option value="__new">+ Novo equipamento</option></select><input id="deviceNew" placeholder="Nome do novo equipamento" style="display:none;margin-top:8px"><label>Botao / comando</label><input id="name" list="names" placeholder="Power, Volume +, Frio 23C"><datalist id="names"><option>Power</option><option>Volume +</option><option>Volume -</option><option>Input</option><option>Menu</option><option>Ligar</option><option>Desligar</option><option>Frio 23C</option><option>Swing</option></datalist><div class="actions"><button onclick="saveCommand()">Salvar ultimo capturado</button><button class="ghost" onclick="closeSave()">Cancelar</button></div></div></div><div id="toast" class="toast"></div>
<script>
const $=id=>document.getElementById(id);let lastSeen=0;
async function api(path,opt){const r=await fetch(path,opt);if(!r.ok)throw new Error('HTTP '+r.status);return r.json()}
function showToast(msg,type='ok'){const t=$('toast');t.textContent=msg;t.style.display='block';t.style.borderColor=type==='bad'?'#ff647c':'#31d583';setTimeout(()=>t.style.display='none',2600)}
function addLog(msg,type='ok'){const e=document.createElement('div');e.className='log';e.innerHTML='<span class="'+type+'">●</span> '+new Date().toLocaleTimeString()+' - '+msg;$('logs').prepend(e)}
function openSave(){$('modal').classList.add('open');toggleNewDevice();$('name').focus()}function closeSave(){$('modal').classList.remove('open')}
function toggleNewDevice(){const isNew=$('deviceSelect').value==='__new';$('deviceNew').style.display=isNew?'block':'none';if(isNew)$('deviceNew').focus()}
function selectedDevice(){return $('deviceSelect').value==='__new'?$('deviceNew').value:$('deviceSelect').value}
function syncDeviceSelect(list){const select=$('deviceSelect'),current=select.value||'TV';const names=['TV','Ar-condicionado','Projetor','Som','Outro'];list.forEach(c=>{if(c.device&&!names.includes(c.device))names.push(c.device)});select.innerHTML=names.map(n=>`<option value="${esc(n)}">${esc(n)}</option>`).join('')+'<option value="__new">+ Novo equipamento</option>';select.value=names.includes(current)?current:'TV';toggleNewDevice()}
async function loadStatus(){try{const s=await api('/api/status');$('wifi').textContent=s.wifiConnected?'OK':'OFF';$('ip').textContent=s.ip;$('count').textContent=s.commandsCount;$('has').textContent=s.hasLastSignal?'Sim':'Nao'}catch(e){$('wifi').textContent='Erro';addLog('conexao perdida','bad')}}
async function loadLastSignal(){try{const s=await api('/api/last');if(!s.hasSignal){$('last').innerHTML='<b>Status</b><span>Nenhum sinal capturado</span>';return}if(s.timestamp!==lastSeen){lastSeen=s.timestamp;addLog('sinal recebido: '+s.protocol)}$('last').innerHTML=`<b>Protocolo</b><span class="badge">${s.protocol}</span><b>Valor</b><span>${s.value||'-'}</span><b>Bits</b><span>${s.bits}</span><b>RAW length</b><span>${s.rawLength}</span><b>Frequencia</b><span>${s.frequency} kHz</span><b>Timestamp</b><span>${s.timestamp}</span>`;$('raw').textContent='RAW preview: ['+(s.rawPreview||[]).join(', ')+']'}catch(e){addLog('erro ao ler ultimo sinal','bad')}}
async function loadCommands(){try{const list=await api('/api/commands');syncDeviceSelect(list);const groups={};list.forEach(c=>(groups[c.device||'Outro']??=[]).push(c));$('commands').innerHTML=list.length?Object.keys(groups).sort().map(dev=>`<section class="device"><div class="devHead"><h3>${esc(dev)}</h3><span class="badge">${groups[dev].length} comandos</span></div><div class="devBody">${groups[dev].map(c=>`<article class="cmd"><h4>${esc(c.name)}</h4><div class="cmdMeta"><span class="badge">${c.protocol}</span><span class="badge">${c.value||'RAW'}</span><span class="badge">${c.bits} bits</span><span class="badge">RAW ${c.rawLength}</span></div><div class="actions"><button onclick="sendCommand('${c.id}')">Enviar</button><button class="danger" onclick="deleteCommand('${c.id}')">Excluir</button></div></article>`).join('')}</div></section>`).join(''):'<p class="sub">Nenhum equipamento cadastrado.</p>'}catch(e){addLog('erro ao listar comandos','bad')}}
function esc(v){const d=document.createElement('div');d.textContent=String(v||'');return d.innerHTML}
async function post(path,data={}){return api(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})}
async function saveCommand(){try{const r=await post('/api/save',{device:selectedDevice(),name:$('name').value});showToast(r.message,r.success?'ok':'bad');addLog(r.message,r.success?'ok':'bad');if(r.success){$('deviceNew').value='';closeSave()}loadCommands();loadStatus()}catch(e){showToast('Erro ao salvar','bad')}}
async function sendLast(){try{const r=await post('/api/send-last');showToast(r.message,r.success?'ok':'bad');addLog(r.message,r.success?'ok':'bad')}catch(e){showToast('Erro ao enviar','bad')}}
async function sendCommand(id){try{const r=await post('/api/send',{id});showToast(r.message,r.success?'ok':'bad');addLog(r.message,r.success?'ok':'bad')}catch(e){showToast('Erro ao enviar','bad')}}
async function deleteCommand(id){try{const r=await post('/api/delete',{id});showToast(r.message,r.success?'ok':'bad');addLog(r.message,r.success?'ok':'bad');loadCommands();loadStatus()}catch(e){showToast('Erro ao excluir','bad')}}
async function clearCommands(){try{const r=await post('/api/clear');showToast(r.message,r.success?'ok':'bad');addLog(r.message,r.success?'ok':'bad');loadCommands();loadStatus()}catch(e){showToast('Erro ao limpar','bad')}}
async function exportCommands(){try{const data=await api('/api/export');const blob=new Blob([JSON.stringify(data,null,2)],{type:'application/json'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='central-ir-esp32.json';a.click();URL.revokeObjectURL(a.href);addLog('esquema exportado')}catch(e){showToast('Erro ao exportar','bad')}}
async function importCommands(file){if(!file)return;try{const data=JSON.parse(await file.text());let ok=0;for(const c of data.commands||data){const r=await post('/api/import-command',{device:c.device||'',name:c.name||'',protocol:c.protocol||'RAW',value:c.value||'',bits:String(c.bits||0),frequency:String(c.frequency||38),rawData:c.rawData||''});if(r.success)ok++}showToast(ok+' comandos importados');addLog(ok+' comandos importados');loadCommands();loadStatus()}catch(e){showToast('Erro ao importar','bad')}}
loadStatus();loadLastSignal();loadCommands();setInterval(loadStatus,1500);setInterval(loadLastSignal,1000);setInterval(loadCommands,3000);
</script></body></html>
)rawliteral";

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

String uint64ToHex(uint64_t value) {
  if (value == 0) return "";
  char buffer[19];
  snprintf(buffer, sizeof(buffer), "0x%llX", (unsigned long long)value);
  return String(buffer);
}

String getBodyValue(const String& key) {
  if (server.hasArg(key)) return server.arg(key);
  String body = server.arg("plain");
  String needle = "\"" + key + "\"";
  int keyPos = body.indexOf(needle);
  if (keyPos < 0) return "";
  int colon = body.indexOf(':', keyPos + needle.length());
  int firstQuote = body.indexOf('"', colon + 1);
  int secondQuote = body.indexOf('"', firstQuote + 1);
  if (colon < 0 || firstQuote < 0 || secondQuote < 0) return "";
  return body.substring(firstQuote + 1, secondQuote);
}

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

void sendJson(uint16_t code, const String& json) {
  Serial.print("HTTP ");
  Serial.print(server.uri());
  Serial.print(" -> ");
  Serial.println(code);
  server.send(code, "application/json", json);
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

void handleLastSignal() {
  sendJson(200, lastSignalToJson());
}

void handleCommands() {
  sendJson(200, commandsToJson());
}

void handleExportCommands() {
  sendJson(200, exportCommandsToJson());
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

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/last", HTTP_GET, handleLastSignal);
  server.on("/api/commands", HTTP_GET, handleCommands);
  server.on("/api/export", HTTP_GET, handleExportCommands);
  server.on("/api/debug", HTTP_GET, handleDebug);
  server.on("/api/save", HTTP_POST, handleSaveCommand);
  server.on("/api/import-command", HTTP_POST, handleImportCommand);
  server.on("/api/send-last", HTTP_POST, handleSendLast);
  server.on("/api/send", HTTP_POST, handleSendCommand);
  server.on("/api/delete", HTTP_POST, handleDeleteCommand);
  server.on("/api/clear", HTTP_POST, handleClearCommands);
  server.begin();
  Serial.println("Servidor web iniciado na porta 80");
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
  captureIR();
}
