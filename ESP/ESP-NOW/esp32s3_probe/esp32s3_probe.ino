/* ESP-NOW range-test PROBE: leaf node. Route is strictly PROBE -> GATE -> BASE. */
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#define ESPNOW_CHANNEL 6
#define DEVICE_ID 3
#define SEND_INTERVAL_MS 500UL
#define LINK_TIMEOUT_MS 2000UL
#define SERIAL_BAUD 115200

#define PROTOCOL_MAGIC 0x4E57
#define PROTOCOL_VERSION 2
#define RSSI_NOT_AVAILABLE 127
#define HELLO_INTERVAL_MS 3000UL
#define PACKET_FLAG_NEW_RTT 0x01

// Set this only when the selected board supplies neither RGB_BUILTIN nor LED_BUILTIN.
#ifndef PROBE_LED_PIN
#define PROBE_LED_PIN -1
#endif
#ifndef PROBE_LED_ACTIVE_HIGH
#define PROBE_LED_ACTIVE_HIGH 1
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#define ESPNOW_RX_INFO_CALLBACK 1
#else
#define ESPNOW_RX_INFO_CALLBACK 0
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#define ESPNOW_TX_INFO_CALLBACK 1
#else
#define ESPNOW_TX_INFO_CALLBACK 0
#endif

enum DeviceId : uint8_t { DEVICE_BASE = 1, DEVICE_GATE = 2, DEVICE_PROBE = 3 };
enum PacketType : uint8_t {
  HELLO = 1, BASE_BEACON = 2, PING = 3, PONG = 4, HEARTBEAT = 5, ACK = 6,
  GATE_BEACON = 7, RELAY_PING = 8, RELAY_PONG = 9,
};

struct __attribute__((packed)) EspNowPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t senderId;
  uint8_t originId;
  uint8_t flags;
  uint8_t hopCount;
  uint32_t sequence;
  uint32_t timestampMs;
  uint32_t replyToSequence;
  uint32_t originSequence;
  uint32_t metricMs;
  int8_t relayRssi;
  uint8_t reserved[3];
};
static_assert(sizeof(EspNowPacket) == 32, "Unexpected packet size");

struct RxEvent {
  uint8_t mac[6];
  int rssi;
  uint16_t dataLength;
  uint8_t data[sizeof(EspNowPacket)];
};

struct LinkStats {
  bool hasExpectedSequence;
  uint32_t expectedSequence;
  uint32_t received;
  uint32_t estimatedLost;
  uint32_t duplicates;
  uint32_t outOfOrder;
  int rssiCurrent;
  int rssiMin;
  int rssiMax;
  int64_t rssiSum;
  uint32_t rssiSamples;
  uint32_t rttCurrent;
  uint32_t rttMin;
  uint32_t rttMax;
  uint64_t rttSum;
  uint32_t rttSamples;
};

const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
QueueHandle_t rxQueue = nullptr;
bool espNowReady = false;
LinkStats gateStats = {};
bool gateKnown = false;
uint8_t gateMac[6] = {};
bool routeOnline = false;
bool gateLinkOnline = false;
bool gateReportsBaseOnline = false;
uint32_t lastValidPongMs = 0;
uint32_t lastGateReplyMs = 0;
uint32_t pendingRouteSequence = 0;
uint32_t pendingSentMs = 0;
bool waitingForPong = false;
uint32_t nextSequence = 1;
uint32_t nextRouteSequence = 1;
uint32_t lastRttMs = UINT32_MAX;
bool rttPendingReport = false;
uint32_t applicationSent = 0;
uint32_t applicationConfirmed = 0;
uint32_t applicationMissed = 0;
uint32_t lastHelloMs = 0;
uint32_t lastPingMs = 0;
uint32_t lastStatusMs = 0;
volatile uint32_t droppedRxEvents = 0;
volatile uint32_t sendCallbackOk = 0;
volatile uint32_t sendCallbackFail = 0;
enum ProbeLedState : uint8_t { PROBE_LED_OFF, PROBE_LED_GREEN, PROBE_LED_YELLOW, PROBE_LED_RED };
ProbeLedState ledState = PROBE_LED_OFF;

const char *packetTypeName(uint8_t type) {
  switch (type) {
    case HELLO: return "HELLO";
    case BASE_BEACON: return "BASE_BEACON";
    case PING: return "PING";
    case PONG: return "PONG";
    case HEARTBEAT: return "HEARTBEAT";
    case ACK: return "ACK";
    case GATE_BEACON: return "GATE_BEACON";
    case RELAY_PING: return "RELAY_PING";
    case RELAY_PONG: return "RELAY_PONG";
    default: return "UNKNOWN";
  }
}

bool isKnownPacketType(uint8_t type) { return type >= HELLO && type <= RELAY_PONG; }
bool hasElapsed(uint32_t now, uint32_t since, uint32_t period) { return (uint32_t)(now - since) >= period; }

void macToText(const uint8_t *mac, char *out, size_t outSize) {
  snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void rssiToText(int rssi, char *out, size_t outSize) {
  if (rssi == RSSI_NOT_AVAILABLE) snprintf(out, outSize, "N/A");
  else snprintf(out, outSize, "%d", rssi);
}

double pdr(const LinkStats &stats) {
  const double total = (double)stats.received + (double)stats.estimatedLost;
  return total == 0.0 ? 0.0 : 100.0 * (double)stats.received / total;
}

void setProbeLed(ProbeLedState state) {
  if (state == ledState) return;
  ledState = state;
#if defined(RGB_BUILTIN)
  uint8_t red = 0;
  uint8_t green = 0;
  if (state == PROBE_LED_GREEN) green = 24;
  else if (state == PROBE_LED_YELLOW) red = green = 20;
  else if (state == PROBE_LED_RED) red = 24;
  rgbLedWrite(RGB_BUILTIN, red, green, 0);
#elif defined(LED_BUILTIN)
  const bool on = state == PROBE_LED_GREEN || state == PROBE_LED_YELLOW;
  digitalWrite(LED_BUILTIN, on == (PROBE_LED_ACTIVE_HIGH != 0) ? HIGH : LOW);
#elif PROBE_LED_PIN >= 0
  const bool on = state == PROBE_LED_GREEN || state == PROBE_LED_YELLOW;
  digitalWrite(PROBE_LED_PIN, on == (PROBE_LED_ACTIVE_HIGH != 0) ? HIGH : LOW);
#else
  (void)state;
#endif
}

void initProbeLed() {
#if defined(RGB_BUILTIN)
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
#elif defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, PROBE_LED_ACTIVE_HIGH ? LOW : HIGH);
#elif PROBE_LED_PIN >= 0
  pinMode(PROBE_LED_PIN, OUTPUT);
  digitalWrite(PROBE_LED_PIN, PROBE_LED_ACTIVE_HIGH ? LOW : HIGH);
#endif
}

bool ensurePeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, sizeof(peerInfo.peer_addr));
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;
  const esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result == ESP_OK || result == ESP_ERR_ESPNOW_EXIST) return true;
  Serial.printf("ERR|ms=%lu|where=ADD_PEER|code=%d\n", (unsigned long)millis(), (int)result);
  return false;
}

EspNowPacket makePacket(PacketType type, uint32_t metricMs = UINT32_MAX, uint8_t flags = 0) {
  EspNowPacket packet = {};
  packet.magic = PROTOCOL_MAGIC;
  packet.version = PROTOCOL_VERSION;
  packet.type = type;
  packet.senderId = DEVICE_ID;
  packet.originId = DEVICE_ID;
  packet.flags = flags;
  packet.sequence = nextSequence++;
  packet.timestampMs = millis();
  packet.metricMs = metricMs;
  packet.relayRssi = RSSI_NOT_AVAILABLE;
  return packet;
}

bool sendRawPacket(const uint8_t *mac, const char *nodeName, const EspNowPacket &packet) {
  const esp_err_t result = esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  char macText[18];
  macToText(mac, macText, sizeof(macText));
  Serial.printf("TX|ms=%lu|node=%s|mac=%s|type=%s|seq=%lu|origin_seq=%lu|status=%s\n",
                (unsigned long)millis(), nodeName, macText, packetTypeName(packet.type),
                (unsigned long)packet.sequence, (unsigned long)packet.originSequence,
                result == ESP_OK ? "QUEUED" : "ERROR");
  return result == ESP_OK;
}

void enqueueRx(const uint8_t *mac, const uint8_t *data, int dataLength, int rssi) {
  if (rxQueue == nullptr) return;
  RxEvent event = {};
  memcpy(event.mac, mac, sizeof(event.mac));
  event.rssi = rssi;
  event.dataLength = dataLength < 0 ? 0 : (uint16_t)dataLength;
  if (dataLength > 0 && dataLength <= (int)sizeof(event.data)) memcpy(event.data, data, dataLength);
  if (xQueueSend(rxQueue, &event, 0) != pdPASS) ++droppedRxEvents;
}

#if ESPNOW_RX_INFO_CALLBACK
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int dataLength) {
  if (info != nullptr) enqueueRx(info->src_addr, data, dataLength,
                                 info->rx_ctrl != nullptr ? info->rx_ctrl->rssi : RSSI_NOT_AVAILABLE);
}
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int dataLength) {
  enqueueRx(mac, data, dataLength, RSSI_NOT_AVAILABLE);
}
#endif

#if ESPNOW_TX_INFO_CALLBACK
void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  (void)info;
  if (status == ESP_NOW_SEND_SUCCESS) ++sendCallbackOk; else ++sendCallbackFail;
}
#else
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  (void)mac;
  if (status == ESP_NOW_SEND_SUCCESS) ++sendCallbackOk; else ++sendCallbackFail;
}
#endif

void updateSequence(uint32_t sequence) {
  if (!gateStats.hasExpectedSequence) {
    ++gateStats.received;
    gateStats.hasExpectedSequence = true;
    gateStats.expectedSequence = sequence + 1;
    return;
  }
  const uint32_t delta = sequence - gateStats.expectedSequence;
  if (delta == 0) { ++gateStats.received; ++gateStats.expectedSequence; }
  else if (delta < 0x80000000UL) {
    ++gateStats.received;
    gateStats.estimatedLost += delta;
    gateStats.expectedSequence = sequence + 1;
  } else if (sequence == gateStats.expectedSequence - 1) ++gateStats.duplicates;
  else ++gateStats.outOfOrder;
}

void updateRssi(int rssi) {
  if (rssi == RSSI_NOT_AVAILABLE) return;
  gateStats.rssiCurrent = rssi;
  if (gateStats.rssiSamples == 0) gateStats.rssiMin = gateStats.rssiMax = rssi;
  else {
    if (rssi < gateStats.rssiMin) gateStats.rssiMin = rssi;
    if (rssi > gateStats.rssiMax) gateStats.rssiMax = rssi;
  }
  gateStats.rssiSum += rssi;
  ++gateStats.rssiSamples;
}

void updateRtt(uint32_t rtt) {
  gateStats.rttCurrent = rtt;
  if (gateStats.rttSamples == 0) gateStats.rttMin = gateStats.rttMax = rtt;
  else {
    if (rtt < gateStats.rttMin) gateStats.rttMin = rtt;
    if (rtt > gateStats.rttMax) gateStats.rttMax = rtt;
  }
  gateStats.rttSum += rtt;
  ++gateStats.rttSamples;
}

void acceptGate(const uint8_t *mac) {
  if (gateKnown && memcmp(gateMac, mac, sizeof(gateMac)) != 0) {
    if (esp_now_is_peer_exist(gateMac)) esp_now_del_peer(gateMac);
    gateStats.hasExpectedSequence = false;
    Serial.printf("EVENT|ms=%lu|node=GATE|state=MAC_CHANGED\n", (unsigned long)millis());
  }
  const bool wasKnown = gateKnown;
  memcpy(gateMac, mac, sizeof(gateMac));
  gateKnown = true;
  if (!wasKnown) {
    char macText[18];
    macToText(gateMac, macText, sizeof(macText));
    Serial.printf("EVENT|ms=%lu|node=GATE|state=DISCOVERED|mac=%s\n",
                  (unsigned long)millis(), macText);
    lastPingMs = millis() - SEND_INTERVAL_MS;
  }
}

void processRx(const RxEvent &event) {
  if (event.dataLength != sizeof(EspNowPacket)) return;
  EspNowPacket packet = {};
  memcpy(&packet, event.data, sizeof(packet));
  if (packet.magic != PROTOCOL_MAGIC || packet.version != PROTOCOL_VERSION ||
      packet.senderId != DEVICE_GATE || !isKnownPacketType(packet.type)) return;

  acceptGate(event.mac);
  if (!ensurePeer(event.mac)) return;
  updateSequence(packet.sequence);
  updateRssi(event.rssi);

  if (packet.type == GATE_BEACON && packet.replyToSequence != 0) {
    lastGateReplyMs = millis();
    gateReportsBaseOnline = (packet.flags & 0x02) != 0;
    if (!gateLinkOnline) {
      gateLinkOnline = true;
      Serial.printf("EVENT|ms=%lu|node=GATE|state=ONLINE\n", (unsigned long)lastGateReplyMs);
    }
    Serial.printf("RX|ms=%lu|node=GATE|hop=PROBE_GATE|type=GATE_STATUS|seq=%lu|rssi=%d|base_route=%s\n",
                  (unsigned long)millis(), (unsigned long)packet.replyToSequence, event.rssi,
                  gateReportsBaseOnline ? "YES" : "NO");
  } else if (packet.type == PONG && packet.originId == DEVICE_PROBE && waitingForPong &&
      packet.originSequence == pendingRouteSequence) {
    const uint32_t rtt = (uint32_t)(millis() - pendingSentMs);
    waitingForPong = false;
    lastValidPongMs = millis();
    lastRttMs = rtt;
    rttPendingReport = true;
    ++applicationConfirmed;
    updateRtt(rtt);
    if (!routeOnline) {
      routeOnline = true;
      Serial.printf("EVENT|ms=%lu|node=PROBE|via=GATE|state=ONLINE\n", (unsigned long)lastValidPongMs);
    }
    Serial.printf("RX|ms=%lu|node=GATE|hop=END_TO_END|type=PONG|seq=%lu|rssi=%d|rtt=%lu\n",
                  (unsigned long)millis(), (unsigned long)packet.originSequence,
                  event.rssi, (unsigned long)rtt);
  } else {
    char rssiText[8];
    rssiToText(event.rssi, rssiText, sizeof(rssiText));
    Serial.printf("RX|ms=%lu|node=GATE|hop=PROBE_GATE|type=%s|seq=%lu|rssi=%s\n",
                  (unsigned long)millis(), packetTypeName(packet.type),
                  (unsigned long)packet.sequence, rssiText);
  }
}

void sendHello(uint32_t now) {
  if (!hasElapsed(now, lastHelloMs, HELLO_INTERVAL_MS)) return;
  lastHelloMs = now;
  sendRawPacket(BROADCAST_MAC, "GATE", makePacket(HELLO));
}

void sendPing(uint32_t now) {
  if (!gateKnown || !hasElapsed(now, lastPingMs, SEND_INTERVAL_MS)) return;
  lastPingMs = now;
  if (waitingForPong) ++applicationMissed;

  const uint8_t flags = rttPendingReport ? PACKET_FLAG_NEW_RTT : 0;
  const uint32_t metric = rttPendingReport ? lastRttMs : UINT32_MAX;
  EspNowPacket ping = makePacket(PING, metric, flags);
  ping.originSequence = nextRouteSequence++;
  const bool queued = sendRawPacket(gateMac, "GATE", ping);
  if (queued) {
    pendingRouteSequence = ping.originSequence;
    pendingSentMs = millis();
    waitingForPong = true;
    ++applicationSent;
    rttPendingReport = false;
  } else {
    waitingForPong = false;
    ++applicationMissed;
  }
}

void updateLinkAndLed(uint32_t now) {
  if (routeOnline && hasElapsed(now, lastValidPongMs, LINK_TIMEOUT_MS)) {
    routeOnline = false;
    Serial.printf("EVENT|ms=%lu|node=PROBE|via=GATE|state=OFFLINE\n", (unsigned long)now);
  }
  if (gateLinkOnline && hasElapsed(now, lastGateReplyMs, LINK_TIMEOUT_MS)) {
    gateLinkOnline = false;
    gateReportsBaseOnline = false;
    Serial.printf("EVENT|ms=%lu|node=GATE|state=OFFLINE\n", (unsigned long)now);
  }

  if (routeOnline) setProbeLed(PROBE_LED_GREEN);
  else if (gateLinkOnline) setProbeLed(PROBE_LED_YELLOW);
  else setProbeLed(PROBE_LED_RED);
}

void printStatus(uint32_t now) {
  char macText[18] = "N/A";
  char rssiText[8];
  if (gateKnown) macToText(gateMac, macText, sizeof(macText));
  rssiToText(gateStats.rssiCurrent, rssiText, sizeof(rssiText));
  const uint32_t lastPong = lastValidPongMs == 0 ? 0 : (uint32_t)(now - lastValidPongMs);
  const uint32_t lastGateReply = lastGateReplyMs == 0 ? 0 : (uint32_t)(now - lastGateReplyMs);
  Serial.printf("STAT|ms=%lu|node=PROBE|hop=END_TO_END|via=GATE|mac=%s|online=%s|gate_link=%s|gate_base=%s|gate_rx=%lu|gate_lost=%lu|gate_rssi=%s|last_pong=%lu|last_gate_reply=%lu",
                (unsigned long)now, macText, routeOnline ? "YES" : "NO",
                gateLinkOnline ? "YES" : "NO", gateReportsBaseOnline ? "YES" : "NO",
                (unsigned long)gateStats.received, (unsigned long)gateStats.estimatedLost,
                rssiText, (unsigned long)lastPong, (unsigned long)lastGateReply);
  if (gateStats.rttSamples > 0) {
    Serial.printf("|rtt=%lu|rtt_min=%lu|rtt_max=%lu|rtt_avg=%.1f",
                  (unsigned long)gateStats.rttCurrent, (unsigned long)gateStats.rttMin,
                  (unsigned long)gateStats.rttMax, (double)gateStats.rttSum / gateStats.rttSamples);
  } else Serial.print("|rtt=N/A|rtt_min=N/A|rtt_max=N/A|rtt_avg=N/A");
  Serial.printf("|app_tx=%lu|app_pong=%lu|app_missed=%lu|queue_drop=%lu\n",
                (unsigned long)applicationSent, (unsigned long)applicationConfirmed,
                (unsigned long)applicationMissed, (unsigned long)droppedRxEvents);
}

bool startEspNow() {
  WiFi.mode(WIFI_STA);
  Serial.printf("CFG|wifi_power_save=%s\n", esp_wifi_set_ps(WIFI_PS_NONE) == ESP_OK ? "OFF" : "UNAVAILABLE");
  if (esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) return false;
  if (esp_now_init() != ESP_OK) return false;
  if (esp_now_register_recv_cb(onDataRecv) != ESP_OK || esp_now_register_send_cb(onDataSent) != ESP_OK) return false;
  return ensurePeer(BROADCAST_MAC);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  initProbeLed();
  gateStats.rssiCurrent = RSSI_NOT_AVAILABLE;
  rxQueue = xQueueCreate(24, sizeof(RxEvent));
  if (rxQueue == nullptr) return;
  Serial.printf("BOOT|node=PROBE|route=GATE_BASE|channel=%d|protocol=%d|idf=%d.%d.%d|rx_rssi=%s\n",
                ESPNOW_CHANNEL, PROTOCOL_VERSION, ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR,
                ESP_IDF_VERSION_PATCH, ESPNOW_RX_INFO_CALLBACK ? "AVAILABLE" : "N/A");
  espNowReady = startEspNow();
  if (espNowReady) lastHelloMs = millis() - HELLO_INTERVAL_MS;
}

void loop() {
  if (!espNowReady) return;
  RxEvent event;
  while (xQueueReceive(rxQueue, &event, 0) == pdPASS) processRx(event);
  const uint32_t now = millis();
  sendHello(now);
  sendPing(now);
  updateLinkAndLed(now);
  if (hasElapsed(now, lastStatusMs, 5000UL)) {
    lastStatusMs = now;
    printStatus(now);
  }
}
