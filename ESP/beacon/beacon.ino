/*
 * ESP32-S3 Beacon Spammer
 * Injeta quadros 802.11 beacon com SSIDs e BSSIDs falsos
 * Técnica usada em pesquisas de segurança wireless
 */

#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

// ─── Configurações ───────────────────────────────────────────────
#define CHANNEL_HOP_INTERVAL_MS  500   // Intervalo para trocar de canal (ms)
#define BEACON_INTERVAL_MS         5   // Intervalo entre beacons (ms)
#define START_CHANNEL              1
#define END_CHANNEL               13

// ─── Lista de SSIDs ──────────────────────────────────────────────
const char* ssids[] = {
  "Van de Vigilancia da PF",
  "Unidade Movel da ABIN",
  "Escritorio da Receita Federal",
  "Nao e sua internet nao",
  "WiFi Gratis do Governo",
  "Aqui tem WiFi sim sinhô",
  "Internet do Vizinho",
  "Pede a senha pro Ze",
  "A LAN antes do tempo",
  "O Silencio dos LANs",
  "Roteador? Eu mal o conheco",
  "Procurando sinal...",
  "Carregando WiFi...",
  "Arruma o seu proprio WiFi",
  "Senha e 1234",
  "WiFi nao e seu nao",
  "Wu-Tang LAN",
  "A Terra Prometida",
  "Getulio Vargas WiFi",
  "Lula Router do Povo",
  "Bolsonaro Wireless",
  "Benjamin FrancisLAN",
  "Quero Internet",
  "O Invernet esta chegando",
  "Jogo de Celular",
  "A Rede Antes Conhecida Como",
  "404 Rede Nao Encontrada",
  "Por Que WiFi Tao Serio?",
  "Diz pro WiFi que eu amo ele",
  "WiFi Onde Estavas?",
  "Definitivamente Nao e Skynet",
  "Skynet Defesa Global",
  "Me Hackeia se for Capaz",
  "Totalmente Seguro kkkk",
  "Virus Detectado!",
  "Atualizacao do Windows",
  "Clique Aqui Bitcoin Gratis",
  "WiFi do Dinheiro",
  "OnlyFans Premium BR",
  "Netflix Plano Gratuito",
  "Amazon Prime de Graca",
  "Uber Eats Sem Taxa",
  "iCloud Desbloqueado BR",
  "Atualizacao Android",
  "Windows Defender WiFi",
  "McAfee Protecao Total",
  "Policia Federal BR",
  "Receita Federal Online",
  "Banco Central do Brasil",
  "WiFi do Hospital",
};

const int SSID_COUNT = sizeof(ssids) / sizeof(ssids[0]);

// ─── Template do quadro beacon 802.11 ───────────────────────────
// O esp_wifi_80211_tx espera o frame 802.11 puro, SEM radiotap header.
// Estrutura: FC(2) + Dur(2) + DA(6) + SA(6) + BSSID(6) + Seq(2)
//          + Timestamp(8) + BI(2) + Cap(2) + SSID_IE + Rates_IE + DS_IE
uint8_t beaconPacket[] = {
  // 802.11 Frame Control (beacon = 0x80 0x00)
  0x80, 0x00,             // type: Management, subtype: Beacon  [0]
  0x00, 0x00,             // duration                           [2]

  // Destination Address: broadcast                             [4]
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

  // Source Address — será preenchido dinamicamente             [10]
  0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,

  // BSSID — será preenchido dinamicamente                      [16]
  0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,

  // Sequence number                                            [22]
  0x00, 0x00,

  // Timestamp (8 bytes) — será preenchido com micros()         [24]
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  // Beacon interval: 100 TU = 102.4ms (little-endian)         [32]
  0x64, 0x00,

  // Capability info: ESS                                       [34]
  0x31, 0x04,

  // SSID Information Element                                   [36]
  0x00,       // Tag: SSID
  0x00,       // Comprimento (preenchido dinamicamente)         [37]
  // SSID (até 32 bytes — preenchido dinamicamente)             [38]
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  // Supported Rates IE                                         [70]
  0x01, 0x08,
  0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C,

  // DS Parameter Set IE (canal — preenchido dinamicamente)     [80]
  0x03, 0x01, 0x01
};

// Offsets dentro do array (sem radiotap)
#define BSSID_OFFSET_SA    10  // Source Address
#define BSSID_OFFSET_BSSID 16  // BSSID field
#define TIMESTAMP_OFFSET   24
#define SSID_LEN_OFFSET    37
#define SSID_DATA_OFFSET   38
#define CHANNEL_OFFSET     82  // DS Parameter Set: canal byte

// ─── Variáveis globais ───────────────────────────────────────────
uint8_t currentChannel = START_CHANNEL;
unsigned long lastChannelHop = 0;
unsigned long lastBeacon = 0;
int ssidIndex = 0;
uint32_t beaconCount = 0;

// ─── Gera MAC aleatório com bit local e unicast corretos ─────────
void randomMAC(uint8_t* mac) {
  for (int i = 0; i < 6; i++) mac[i] = esp_random() & 0xFF;
  mac[0] = (mac[0] & 0xFE) | 0x02; // locally administered, unicast
}

// ─── Monta e injeta um quadro beacon ────────────────────────────
void sendBeacon(const char* ssid, uint8_t channel) {
  // Buffer dinâmico com tamanho exato
  uint8_t ssidLen = (uint8_t)min((int)strlen(ssid), 32);
  // Estrutura: frame fixo (36 bytes até SSID_DATA) + ssid + Rates IE (10) + DS IE (3)
  int pktSize = SSID_DATA_OFFSET + ssidLen + 10 + 3;
  uint8_t packet[pktSize];
  memcpy(packet, beaconPacket, SSID_DATA_OFFSET + 32); // copia template até fim do SSID slot

  // BSSID aleatório
  uint8_t mac[6];
  randomMAC(mac);
  memcpy(&packet[BSSID_OFFSET_SA],    mac, 6);
  memcpy(&packet[BSSID_OFFSET_BSSID], mac, 6);

  // Timestamp
  uint64_t ts = (uint64_t)micros();
  memcpy(&packet[TIMESTAMP_OFFSET], &ts, 8);

  // SSID IE
  packet[SSID_LEN_OFFSET] = ssidLen;
  memcpy(&packet[SSID_DATA_OFFSET], ssid, ssidLen);

  // Supported Rates IE: 0x01 0x08 + 8 taxas
  int ratesOffset = SSID_DATA_OFFSET + ssidLen;
  packet[ratesOffset + 0] = 0x01; packet[ratesOffset + 1] = 0x08;
  packet[ratesOffset + 2] = 0x82; packet[ratesOffset + 3] = 0x84;
  packet[ratesOffset + 4] = 0x8B; packet[ratesOffset + 5] = 0x96;
  packet[ratesOffset + 6] = 0x24; packet[ratesOffset + 7] = 0x30;
  packet[ratesOffset + 8] = 0x48; packet[ratesOffset + 9] = 0x6C;

  // DS Parameter Set IE: tag, len, canal
  int dsOffset = ratesOffset + 10;
  packet[dsOffset + 0] = 0x03;
  packet[dsOffset + 1] = 0x01;
  packet[dsOffset + 2] = channel;

  esp_wifi_80211_tx(WIFI_IF_AP, packet, pktSize, false);
}

// ─── Setup ───────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-S3 Beacon Spammer ===");
  Serial.printf("SSIDs carregados: %d\n", SSID_COUNT);

  // Inicializa NVS, netif e event loop (necessário para esp_wifi)
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();

  // Configura Wi-Fi em modo AP para permitir injeção de frames
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_AP);

  wifi_config_t apConfig = {};
  strncpy((char*)apConfig.ap.ssid, "ESP32S3", sizeof(apConfig.ap.ssid));
  apConfig.ap.ssid_len     = 7;
  apConfig.ap.channel      = START_CHANNEL;
  apConfig.ap.authmode     = WIFI_AUTH_OPEN;
  apConfig.ap.max_connection = 0;
  esp_wifi_set_config(WIFI_IF_AP, &apConfig);

  esp_wifi_start();

  // Permite injeção de quadros brutos
  esp_wifi_set_promiscuous(true);

  Serial.println("Wi-Fi iniciado. Iniciando spam de beacons...\n");
}

// ─── Loop ────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Envia beacon para o SSID atual
  if (now - lastBeacon >= BEACON_INTERVAL_MS) {
    lastBeacon = now;
    sendBeacon(ssids[ssidIndex], currentChannel);
    ssidIndex = (ssidIndex + 1) % SSID_COUNT;
    beaconCount++;
  }

  // Rotaciona o canal Wi-Fi periodicamente
  if (now - lastChannelHop >= CHANNEL_HOP_INTERVAL_MS) {
    lastChannelHop = now;
    currentChannel++;
    if (currentChannel > END_CHANNEL) currentChannel = START_CHANNEL;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    Serial.printf("[Canal %2d] Beacons enviados: %lu\n", currentChannel, beaconCount);
  }
}
