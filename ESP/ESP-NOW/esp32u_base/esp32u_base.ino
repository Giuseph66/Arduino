/*
 * ESP-NOW range-test BASE
 * ESP32U in the office. Opens in Arduino IDE as esp32u_base.ino.
 */
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

// Configuration shared by all laboratory sketches.
#define ESPNOW_CHANNEL 6
#define DEVICE_ID 1
#define SEND_INTERVAL_MS 1000UL
#define LINK_TIMEOUT_MS 3000UL
#define SERIAL_BAUD 115200

#define PROTOCOL_MAGIC 0x4E57  // "NW"
#define PROTOCOL_VERSION 2
#define RSSI_NOT_AVAILABLE 127
#define PACKET_FLAG_NEW_RTT 0x01

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#define ESPNOW_RX_INFO_CALLBACK 1
#else
#define ESPNOW_RX_INFO_CALLBACK 0
#endif

// ESP-IDF changed the send callback later than the receive callback.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#define ESPNOW_TX_INFO_CALLBACK 1
#else
#define ESPNOW_TX_INFO_CALLBACK 0
#endif

enum DeviceId : uint8_t { DEVICE_BASE = 1, DEVICE_GATE = 2, DEVICE_PROBE = 3 };
enum PacketType : uint8_t {
  HELLO = 1,
  BASE_BEACON = 2,
  PING = 3,
  PONG = 4,
  HEARTBEAT = 5,
  ACK = 6,
  GATE_BEACON = 7,
  RELAY_PING = 8,
  RELAY_PONG = 9,
};

// Fixed 32-byte protocol. RELAY_* keeps origin sequence across two hops.
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

struct NodeStats {
  const char *name;
  uint8_t id;
  bool known;
  bool online;
  uint8_t mac[6];
  uint32_t lastSeenMs;
  uint32_t lastConfirmedMs;
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
volatile uint32_t sendCallbackOk = 0;
volatile uint32_t sendCallbackFail = 0;
volatile uint32_t droppedRxEvents = 0;
uint32_t nextSequence = 1;
uint32_t lastBeaconMs = 0;
uint32_t lastReportMs = 0;

NodeStats probe = {"PROBE", DEVICE_PROBE};
NodeStats gate = {"GATE", DEVICE_GATE};

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

bool isKnownPacketType(uint8_t type) {
  return type >= HELLO && type <= RELAY_PONG;
}

void macToText(const uint8_t *mac, char *out, size_t outSize) {
  snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void rssiToText(int rssi, char *out, size_t outSize) {
  if (rssi == RSSI_NOT_AVAILABLE) {
    snprintf(out, outSize, "N/A");
  } else {
    snprintf(out, outSize, "%d", rssi);
  }
}

bool hasElapsed(uint32_t now, uint32_t since, uint32_t period) {
  return (uint32_t)(now - since) >= period;
}

double pdr(const NodeStats &node) {
  const double total = (double)node.received + (double)node.estimatedLost;
  return total == 0.0 ? 0.0 : (100.0 * (double)node.received / total);
}

bool ensurePeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, sizeof(peerInfo.peer_addr));
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;  // Deliberately unencrypted for the baseline test.

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result == ESP_OK || result == ESP_ERR_ESPNOW_EXIST) return true;

  char macText[18];
  macToText(mac, macText, sizeof(macText));
  Serial.printf("ERR|ms=%lu|where=ADD_PEER|mac=%s|code=%d\n",
                (unsigned long)millis(), macText, (int)result);
  return false;
}

EspNowPacket makePacket(PacketType type, uint32_t replyToSequence = 0,
                         uint32_t metricMs = UINT32_MAX, uint8_t flags = 0) {
  EspNowPacket packet = {};
  packet.magic = PROTOCOL_MAGIC;
  packet.version = PROTOCOL_VERSION;
  packet.type = type;
  packet.senderId = DEVICE_ID;
  packet.originId = DEVICE_ID;
  packet.flags = flags;
  packet.hopCount = 0;
  packet.sequence = nextSequence++;
  packet.timestampMs = millis();
  packet.replyToSequence = replyToSequence;
  packet.metricMs = metricMs;
  packet.relayRssi = RSSI_NOT_AVAILABLE;
  return packet;
}

bool sendRawPacket(const uint8_t *mac, const char *nodeName, const EspNowPacket &packet) {
  esp_err_t result = esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  char macText[18];
  macToText(mac, macText, sizeof(macText));
  Serial.printf("TX|ms=%lu|node=%s|mac=%s|type=%s|seq=%lu|reply_to=%lu|status=%s\n",
                (unsigned long)millis(), nodeName, macText, packetTypeName(packet.type),
                (unsigned long)packet.sequence, (unsigned long)packet.replyToSequence,
                result == ESP_OK ? "QUEUED" : "ERROR");
  return result == ESP_OK;
}

bool sendPacket(const uint8_t *mac, const char *nodeName, PacketType type,
                uint32_t replyToSequence = 0, uint32_t metricMs = UINT32_MAX,
                uint8_t flags = 0) {
  return sendRawPacket(mac, nodeName, makePacket(type, replyToSequence, metricMs, flags));
}

void enqueueRx(const uint8_t *mac, const uint8_t *data, int dataLength, int rssi) {
  if (rxQueue == nullptr) return;
  RxEvent event = {};
  memcpy(event.mac, mac, sizeof(event.mac));
  event.rssi = rssi;
  event.dataLength = dataLength < 0 ? 0 : (uint16_t)dataLength;
  if (dataLength > 0 && dataLength <= (int)sizeof(event.data)) {
    memcpy(event.data, data, dataLength);
  }
  if (xQueueSend(rxQueue, &event, 0) != pdPASS) ++droppedRxEvents;
}

#if ESPNOW_RX_INFO_CALLBACK
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int dataLength) {
  const int rssi = (info != nullptr && info->rx_ctrl != nullptr)
                     ? info->rx_ctrl->rssi : RSSI_NOT_AVAILABLE;
  if (info != nullptr) enqueueRx(info->src_addr, data, dataLength, rssi);
}
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int dataLength) {
  enqueueRx(mac, data, dataLength, RSSI_NOT_AVAILABLE);
}
#endif

#if ESPNOW_TX_INFO_CALLBACK
void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  (void)info;
  if (status == ESP_NOW_SEND_SUCCESS) ++sendCallbackOk;
  else ++sendCallbackFail;
}
#else
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  (void)mac;
  if (status == ESP_NOW_SEND_SUCCESS) ++sendCallbackOk;
  else ++sendCallbackFail;
}
#endif

void rememberNode(NodeStats &node, const uint8_t *mac) {
  if (node.known && memcmp(node.mac, mac, sizeof(node.mac)) != 0) {
    char oldMac[18];
    macToText(node.mac, oldMac, sizeof(oldMac));
    if (esp_now_is_peer_exist(node.mac)) esp_now_del_peer(node.mac);
    Serial.printf("EVENT|ms=%lu|node=%s|state=MAC_CHANGED|old_mac=%s\n",
                  (unsigned long)millis(), node.name, oldMac);
    node.hasExpectedSequence = false;
  }
  node.known = true;
  memcpy(node.mac, mac, sizeof(node.mac));
}

void updateSequence(NodeStats &node, uint32_t sequence) {
  if (!node.hasExpectedSequence) {
    ++node.received;
    node.hasExpectedSequence = true;
    node.expectedSequence = sequence + 1;
    return;
  }

  const uint32_t delta = sequence - node.expectedSequence;
  if (delta == 0) {
    ++node.received;
    ++node.expectedSequence;
  } else if (delta < 0x80000000UL) {
    ++node.received;
    node.estimatedLost += delta;
    node.expectedSequence = sequence + 1;
  } else if (sequence == node.expectedSequence - 1) {
    ++node.duplicates;
  } else {
    ++node.outOfOrder;
  }
}

void updateRssi(NodeStats &node, int rssi) {
  if (rssi == RSSI_NOT_AVAILABLE) return;
  node.rssiCurrent = rssi;
  if (node.rssiSamples == 0) {
    node.rssiMin = rssi;
    node.rssiMax = rssi;
  } else {
    if (rssi < node.rssiMin) node.rssiMin = rssi;
    if (rssi > node.rssiMax) node.rssiMax = rssi;
  }
  node.rssiSum += rssi;
  ++node.rssiSamples;
}

void updateReportedRtt(NodeStats &node, uint32_t rtt) {
  if (rtt == UINT32_MAX) return;
  node.rttCurrent = rtt;
  if (node.rttSamples == 0) {
    node.rttMin = rtt;
    node.rttMax = rtt;
  } else {
    if (rtt < node.rttMin) node.rttMin = rtt;
    if (rtt > node.rttMax) node.rttMax = rtt;
  }
  node.rttSum += rtt;
  ++node.rttSamples;
}

void updateNodeReception(NodeStats &node, uint32_t sequence, uint8_t flags,
                         uint32_t metricMs, int rssi, bool allowConfirmation) {
  const bool confirmedThisPacket =
      allowConfirmation && (flags & PACKET_FLAG_NEW_RTT) && metricMs != UINT32_MAX;
  updateSequence(node, sequence);
  updateRssi(node, rssi);
  if (allowConfirmation && (flags & PACKET_FLAG_NEW_RTT)) updateReportedRtt(node, metricMs);
  node.lastSeenMs = millis();
  // A freshly reported RTT proves this node received the preceding PONG/ACK.
  // Base's ONLINE state is therefore bidirectional application confirmation.
  if (confirmedThisPacket) {
    node.lastConfirmedMs = node.lastSeenMs;
  }
  if (!node.online && confirmedThisPacket) {
    node.online = true;
    Serial.printf("EVENT|ms=%lu|node=%s|state=ONLINE\n",
                  (unsigned long)node.lastSeenMs, node.name);
  }
}

void logRx(const NodeStats &node, const EspNowPacket &packet, int rssi, const uint8_t *mac) {
  char macText[18];
  char rssiText[8];
  macToText(mac, macText, sizeof(macText));
  rssiToText(rssi, rssiText, sizeof(rssiText));
  if (!(packet.flags & PACKET_FLAG_NEW_RTT) || packet.metricMs == UINT32_MAX) {
    Serial.printf("RX|ms=%lu|node=%s|hop=GATE_BASE|mac=%s|type=%s|seq=%lu|rssi=%s|rtt=N/A\n",
                  (unsigned long)millis(), node.name, macText, packetTypeName(packet.type),
                  (unsigned long)packet.sequence, rssiText);
  } else {
    Serial.printf("RX|ms=%lu|node=%s|hop=GATE_BASE|mac=%s|type=%s|seq=%lu|rssi=%s|rtt=%lu\n",
                  (unsigned long)millis(), node.name, macText, packetTypeName(packet.type),
                  (unsigned long)packet.sequence, rssiText, (unsigned long)packet.metricMs);
  }
}

void logRelayedProbe(const EspNowPacket &packet, int baseRssi, const uint8_t *gateMac) {
  char macText[18];
  char probeHopRssi[8];
  char baseHopRssi[8];
  macToText(gateMac, macText, sizeof(macText));
  rssiToText(packet.relayRssi, probeHopRssi, sizeof(probeHopRssi));
  rssiToText(baseRssi, baseHopRssi, sizeof(baseHopRssi));
  if (!(packet.flags & PACKET_FLAG_NEW_RTT) || packet.metricMs == UINT32_MAX) {
    Serial.printf("RX|ms=%lu|node=PROBE|hop=END_TO_END|via=GATE|mac=%s|type=RELAY_PING|seq=%lu|hop_seq=%lu|rssi=%s|base_rssi=%s|rtt=N/A\n",
                  (unsigned long)millis(), macText, (unsigned long)packet.originSequence,
                  (unsigned long)packet.sequence, probeHopRssi, baseHopRssi);
  } else {
    Serial.printf("RX|ms=%lu|node=PROBE|hop=END_TO_END|via=GATE|mac=%s|type=RELAY_PING|seq=%lu|hop_seq=%lu|rssi=%s|base_rssi=%s|rtt=%lu\n",
                  (unsigned long)millis(), macText, (unsigned long)packet.originSequence,
                  (unsigned long)packet.sequence, probeHopRssi, baseHopRssi,
                  (unsigned long)packet.metricMs);
  }
}

void processRx(const RxEvent &event) {
  if (event.dataLength != sizeof(EspNowPacket)) {
    Serial.printf("DROP|ms=%lu|reason=SIZE|size=%u\n",
                  (unsigned long)millis(), event.dataLength);
    return;
  }

  EspNowPacket packet = {};
  memcpy(&packet, event.data, sizeof(packet));
  if (packet.magic != PROTOCOL_MAGIC || packet.version != PROTOCOL_VERSION) {
    Serial.printf("DROP|ms=%lu|reason=PROTOCOL|magic=%u|version=%u\n",
                  (unsigned long)millis(), packet.magic, packet.version);
    return;
  }
  if (!isKnownPacketType(packet.type)) {
    Serial.printf("DROP|ms=%lu|reason=UNKNOWN_TYPE|type=%u\n",
                  (unsigned long)millis(), packet.type);
    return;
  }

  // Relay-only topology: BASE accepts direct frames only from GATE.
  if (packet.senderId != DEVICE_GATE) {
    Serial.printf("DROP|ms=%lu|reason=DIRECT_ROUTE_DISABLED|sender=%u\n",
                  (unsigned long)millis(), packet.senderId);
    return;
  }

  rememberNode(gate, event.mac);
  if (!ensurePeer(event.mac)) return;
  const bool gateConfirmation = packet.type == HEARTBEAT;
  updateNodeReception(gate, packet.sequence, packet.flags, packet.metricMs,
                      event.rssi, gateConfirmation);
  logRx(gate, packet, event.rssi, event.mac);

  if (packet.type == HELLO) {
    sendPacket(event.mac, "GATE", BASE_BEACON);
  } else if (packet.type == HEARTBEAT) {
    sendPacket(event.mac, "GATE", ACK, packet.sequence);
  } else if (packet.type == RELAY_PING && packet.originId == DEVICE_PROBE) {
    // `relayRssi` is measured by GATE when it received PROBE's PING.
    probe.known = true;
    memcpy(probe.mac, event.mac, sizeof(probe.mac));  // Gateway MAC; status says via=GATE.
    updateNodeReception(probe, packet.originSequence, packet.flags, packet.metricMs,
                        packet.relayRssi, true);
    logRelayedProbe(packet, event.rssi, event.mac);

    EspNowPacket pong = makePacket(RELAY_PONG);
    pong.originId = DEVICE_PROBE;
    pong.originSequence = packet.originSequence;
    pong.hopCount = packet.hopCount + 1;
    sendRawPacket(event.mac, "GATE", pong);
  }
}

void printNodeStat(const NodeStats &node, uint32_t now, const char *hop, const char *via = nullptr) {
  char macText[18] = "N/A";
  char rssiText[8];
  if (node.known) macToText(node.mac, macText, sizeof(macText));
  rssiToText(node.rssiCurrent, rssiText, sizeof(rssiText));
  const uint32_t lastSeen = node.known ? (uint32_t)(now - node.lastSeenMs) : 0;
  Serial.printf("STAT|ms=%lu|node=%s|hop=%s", (unsigned long)now, node.name, hop);
  if (via != nullptr) Serial.printf("|via=%s", via);
  Serial.printf("|mac=%s|online=%s|rx=%lu|lost=%lu|dup=%lu|ooo=%lu|pdr=%.2f|rssi=%s|last_seen=%lu",
                macText, node.online ? "YES" : "NO",
                (unsigned long)node.received, (unsigned long)node.estimatedLost,
                (unsigned long)node.duplicates, (unsigned long)node.outOfOrder,
                pdr(node), rssiText, (unsigned long)lastSeen);
  if (node.rttSamples > 0) {
    Serial.printf("|rtt=%lu|rtt_avg=%.1f", (unsigned long)node.rttCurrent,
                  (double)node.rttSum / node.rttSamples);
  } else {
    Serial.print("|rtt=N/A|rtt_avg=N/A");
  }
  Serial.println();
}

void printHumanNode(const NodeStats &node, uint32_t now) {
  Serial.println(node.name);
  if (!node.known) {
    Serial.println("MAC: N/A\nONLINE: NO\n");
    return;
  }
  char macText[18];
  macToText(node.mac, macText, sizeof(macText));
  Serial.printf("MAC: %s\n", macText);
  if (node.id == DEVICE_PROBE) Serial.println("VIA: GATE");
  Serial.printf("ONLINE: %s\n", node.online ? "YES" : "NO");
  if (node.rssiSamples == 0) {
    Serial.println("RSSI CURRENT: N/A\nRSSI AVG: N/A");
  } else {
    Serial.printf("RSSI CURRENT: %d dBm\nRSSI AVG: %.1f dBm\n", node.rssiCurrent,
                  (double)node.rssiSum / node.rssiSamples);
  }
  Serial.printf("RX: %lu\nLOST: %lu\nPDR: %.2f%%\nLAST SEEN: %lu ms\n\n",
                (unsigned long)node.received, (unsigned long)node.estimatedLost, pdr(node),
                (unsigned long)(now - node.lastSeenMs));
}

void printReport(uint32_t now) {
  printNodeStat(probe, now, "END_TO_END", "GATE");
  printNodeStat(gate, now, "GATE_BASE");
  Serial.println("========== ESP-NOW STATUS ==========");
  printHumanNode(probe, now);
  printHumanNode(gate, now);
  Serial.printf("QUEUE_DROPS: %lu | TX_CALLBACK_OK: %lu | TX_CALLBACK_FAIL: %lu\n",
                (unsigned long)droppedRxEvents, (unsigned long)sendCallbackOk,
                (unsigned long)sendCallbackFail);
  Serial.println("====================================");
}

void updateOnlineState(NodeStats &node, uint32_t now) {
  if (node.known && node.online && hasElapsed(now, node.lastConfirmedMs, LINK_TIMEOUT_MS)) {
    node.online = false;
    Serial.printf("EVENT|ms=%lu|node=%s|state=OFFLINE\n", (unsigned long)now, node.name);
  }
}

bool startEspNow() {
  WiFi.mode(WIFI_STA);
  esp_err_t result = esp_wifi_set_ps(WIFI_PS_NONE);
  Serial.printf("CFG|wifi_power_save=%s\n", result == ESP_OK ? "OFF" : "UNAVAILABLE");
  result = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (result != ESP_OK) {
    Serial.printf("ERR|where=SET_CHANNEL|code=%d\n", (int)result);
    return false;
  }
  result = esp_now_init();
  if (result != ESP_OK) {
    Serial.printf("ERR|where=ESPNOW_INIT|code=%d\n", (int)result);
    return false;
  }
  if (esp_now_register_recv_cb(onDataRecv) != ESP_OK ||
      esp_now_register_send_cb(onDataSent) != ESP_OK) {
    Serial.println("ERR|where=REGISTER_CALLBACK");
    return false;
  }
  return ensurePeer(BROADCAST_MAC);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  probe.rssiCurrent = RSSI_NOT_AVAILABLE;
  gate.rssiCurrent = RSSI_NOT_AVAILABLE;
  rxQueue = xQueueCreate(24, sizeof(RxEvent));
  if (rxQueue == nullptr) {
    Serial.println("ERR|where=RX_QUEUE");
    return;
  }
  Serial.printf("BOOT|node=BASE|channel=%d|protocol=%d|idf=%d.%d.%d|rx_rssi=%s\n",
                ESPNOW_CHANNEL, PROTOCOL_VERSION, ESP_IDF_VERSION_MAJOR,
                ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH,
                ESPNOW_RX_INFO_CALLBACK ? "AVAILABLE" : "N/A");
  espNowReady = startEspNow();
  if (!espNowReady) return;
  lastBeaconMs = millis() - SEND_INTERVAL_MS;
}

void loop() {
  if (!espNowReady) return;
  RxEvent event;
  while (rxQueue != nullptr && xQueueReceive(rxQueue, &event, 0) == pdPASS) processRx(event);

  const uint32_t now = millis();
  if (hasElapsed(now, lastBeaconMs, SEND_INTERVAL_MS)) {
    lastBeaconMs = now;
    sendPacket(BROADCAST_MAC, "ALL", BASE_BEACON);
  }
  updateOnlineState(probe, now);
  updateOnlineState(gate, now);
  if (hasElapsed(now, lastReportMs, 5000UL)) {
    lastReportMs = now;
    printReport(now);
  }
}
