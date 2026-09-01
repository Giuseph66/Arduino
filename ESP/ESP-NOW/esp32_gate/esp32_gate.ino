/*
 * ESP-NOW range-test GATE
 * Normal ESP32 at the gate. Opens in Arduino IDE as esp32_gate.ino.
 */
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#define ESPNOW_CHANNEL 6
#define DEVICE_ID 2
#define SEND_INTERVAL_MS 1000UL  // HEARTBEAT interval
#define LINK_TIMEOUT_MS 3000UL
#define SERIAL_BAUD 115200

#define PROTOCOL_MAGIC 0x4E57
#define PROTOCOL_VERSION 2
#define RSSI_NOT_AVAILABLE 127
#define HELLO_INTERVAL_MS 3000UL
#define PACKET_FLAG_NEW_RTT 0x01

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
LinkStats baseStats = {};
LinkStats probeStats = {};
bool baseKnown = false;
bool baseOnline = false;
uint8_t baseMac[6] = {};
bool probeKnown = false;
uint8_t probeMac[6] = {};
uint32_t lastProbePingMs = 0;
uint32_t lastBasePacketMs = 0;
uint32_t lastValidAckMs = 0;
uint32_t pendingSequence = 0;
uint32_t pendingSentMs = 0;
bool waitingForAck = false;
uint32_t lastRttMs = UINT32_MAX;
bool rttPendingReport = false;
uint32_t applicationSent = 0;
uint32_t applicationConfirmed = 0;
uint32_t applicationMissed = 0;
uint32_t nextSequence = 1;
uint32_t lastHelloMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastGateBeaconMs = 0;
uint32_t lastStatusMs = 0;
volatile uint32_t droppedRxEvents = 0;
volatile uint32_t sendCallbackOk = 0;
volatile uint32_t sendCallbackFail = 0;

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
  if (rssi == RSSI_NOT_AVAILABLE) snprintf(out, outSize, "N/A");
  else snprintf(out, outSize, "%d", rssi);
}

bool hasElapsed(uint32_t now, uint32_t since, uint32_t period) {
  return (uint32_t)(now - since) >= period;
}

double pdr(const LinkStats &stats) {
  const double total = (double)stats.received + (double)stats.estimatedLost;
  return total == 0.0 ? 0.0 : 100.0 * (double)stats.received / total;
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

bool sendRawPacket(const uint8_t *mac, const char *nodeName, const EspNowPacket &packet,
                   uint32_t *sentSequence = nullptr) {
  const esp_err_t result = esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  if (sentSequence != nullptr) *sentSequence = packet.sequence;
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
                uint8_t flags = 0, uint32_t *sentSequence = nullptr) {
  return sendRawPacket(mac, nodeName, makePacket(type, replyToSequence, metricMs, flags),
                       sentSequence);
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
  const int rssi = (info != nullptr && info->rx_ctrl != nullptr) ? info->rx_ctrl->rssi : RSSI_NOT_AVAILABLE;
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
  if (status == ESP_NOW_SEND_SUCCESS) ++sendCallbackOk; else ++sendCallbackFail;
}
#else
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  (void)mac;
  if (status == ESP_NOW_SEND_SUCCESS) ++sendCallbackOk; else ++sendCallbackFail;
}
#endif

void updateSequence(LinkStats &stats, uint32_t sequence) {
  if (!stats.hasExpectedSequence) {
    ++stats.received;
    stats.hasExpectedSequence = true;
    stats.expectedSequence = sequence + 1;
    return;
  }
  const uint32_t delta = sequence - stats.expectedSequence;
  if (delta == 0) {
    ++stats.received;
    ++stats.expectedSequence;
  }
  else if (delta < 0x80000000UL) {
    ++stats.received;
    stats.estimatedLost += delta;
    stats.expectedSequence = sequence + 1;
  } else if (sequence == stats.expectedSequence - 1) ++stats.duplicates;
  else ++stats.outOfOrder;
}

void updateRssi(LinkStats &stats, int rssi) {
  if (rssi == RSSI_NOT_AVAILABLE) return;
  stats.rssiCurrent = rssi;
  if (stats.rssiSamples == 0) stats.rssiMin = stats.rssiMax = rssi;
  else {
    if (rssi < stats.rssiMin) stats.rssiMin = rssi;
    if (rssi > stats.rssiMax) stats.rssiMax = rssi;
  }
  stats.rssiSum += rssi;
  ++stats.rssiSamples;
}

void updateRtt(uint32_t rtt) {
  baseStats.rttCurrent = rtt;
  if (baseStats.rttSamples == 0) baseStats.rttMin = baseStats.rttMax = rtt;
  else {
    if (rtt < baseStats.rttMin) baseStats.rttMin = rtt;
    if (rtt > baseStats.rttMax) baseStats.rttMax = rtt;
  }
  baseStats.rttSum += rtt;
  ++baseStats.rttSamples;
}

void acceptBase(const uint8_t *mac) {
  if (baseKnown && memcmp(baseMac, mac, sizeof(baseMac)) != 0) {
    char oldMac[18];
    macToText(baseMac, oldMac, sizeof(oldMac));
    if (esp_now_is_peer_exist(baseMac)) esp_now_del_peer(baseMac);
    baseStats.hasExpectedSequence = false;
    Serial.printf("EVENT|ms=%lu|node=BASE|state=MAC_CHANGED|old_mac=%s\n",
                  (unsigned long)millis(), oldMac);
  }
  const bool wasKnown = baseKnown;
  memcpy(baseMac, mac, sizeof(baseMac));
  baseKnown = true;
  if (!wasKnown) {
    char macText[18];
    macToText(baseMac, macText, sizeof(macText));
    Serial.printf("EVENT|ms=%lu|node=BASE|state=DISCOVERED|mac=%s\n",
                  (unsigned long)millis(), macText);
    lastHeartbeatMs = millis() - SEND_INTERVAL_MS;
  }
}

void acceptProbe(const uint8_t *mac) {
  if (probeKnown && memcmp(probeMac, mac, sizeof(probeMac)) != 0) {
    if (esp_now_is_peer_exist(probeMac)) esp_now_del_peer(probeMac);
    probeStats.hasExpectedSequence = false;
    Serial.printf("EVENT|ms=%lu|node=PROBE|state=MAC_CHANGED\n", (unsigned long)millis());
  }
  const bool wasKnown = probeKnown;
  memcpy(probeMac, mac, sizeof(probeMac));
  probeKnown = true;
  if (!wasKnown) {
    char macText[18];
    macToText(probeMac, macText, sizeof(macText));
    Serial.printf("EVENT|ms=%lu|node=PROBE|state=DISCOVERED|mac=%s\n",
                  (unsigned long)millis(), macText);
  }
}

void logBaseRx(const EspNowPacket &packet, int rssi, const uint8_t *mac, const char *extra = "") {
  char macText[18];
  char rssiText[8];
  macToText(mac, macText, sizeof(macText));
  rssiToText(rssi, rssiText, sizeof(rssiText));
  Serial.printf("RX|ms=%lu|node=BASE|mac=%s|type=%s|seq=%lu|rssi=%s%s\n",
                (unsigned long)millis(), macText, packetTypeName(packet.type),
                (unsigned long)packet.sequence, rssiText, extra);
}

void logProbeRx(const EspNowPacket &packet, int rssi, const uint8_t *mac) {
  char macText[18];
  char rssiText[8];
  macToText(mac, macText, sizeof(macText));
  rssiToText(rssi, rssiText, sizeof(rssiText));
  Serial.printf("RX|ms=%lu|node=PROBE|hop=PROBE_GATE|mac=%s|type=%s|seq=%lu|rssi=%s\n",
                (unsigned long)millis(), macText, packetTypeName(packet.type),
                (unsigned long)packet.originSequence, rssiText);
}

void sendProbeStatus(const uint8_t *mac, uint32_t replyToSequence, uint32_t originSequence = 0) {
  EspNowPacket status = makePacket(GATE_BEACON, replyToSequence);
  status.originId = DEVICE_PROBE;
  status.originSequence = originSequence;
  if (baseOnline) status.flags |= 0x02;
  sendRawPacket(mac, "PROBE", status);
}

void processRx(const RxEvent &event) {
  if (event.dataLength != sizeof(EspNowPacket)) {
    Serial.printf("DROP|ms=%lu|reason=SIZE|size=%u\n", (unsigned long)millis(), event.dataLength);
    return;
  }
  EspNowPacket packet = {};
  memcpy(&packet, event.data, sizeof(packet));
  if (packet.magic != PROTOCOL_MAGIC || packet.version != PROTOCOL_VERSION ||
      !isKnownPacketType(packet.type)) {
    Serial.printf("DROP|ms=%lu|reason=PROTOCOL_OR_SENDER\n", (unsigned long)millis());
    return;
  }

  if (packet.senderId == DEVICE_BASE) {
    acceptBase(event.mac);
    if (!ensurePeer(event.mac)) return;
    updateSequence(baseStats, packet.sequence);
    updateRssi(baseStats, event.rssi);
    lastBasePacketMs = millis();

    if (packet.type == ACK && waitingForAck && packet.replyToSequence == pendingSequence) {
      const uint32_t rtt = (uint32_t)(millis() - pendingSentMs);
      waitingForAck = false;
      lastValidAckMs = millis();
      lastRttMs = rtt;
      rttPendingReport = true;
      ++applicationConfirmed;
      updateRtt(rtt);
      if (!baseOnline) {
        baseOnline = true;
        Serial.printf("EVENT|ms=%lu|node=BASE|state=ONLINE\n", (unsigned long)lastValidAckMs);
      }
      char extra[32];
      snprintf(extra, sizeof(extra), "|rtt=%lu", (unsigned long)rtt);
      logBaseRx(packet, event.rssi, event.mac, extra);
    } else if (packet.type == RELAY_PONG && packet.originId == DEVICE_PROBE && probeKnown) {
      EspNowPacket pong = makePacket(PONG);
      pong.originId = DEVICE_PROBE;
      pong.originSequence = packet.originSequence;
      pong.hopCount = packet.hopCount + 1;
      sendRawPacket(probeMac, "PROBE", pong);
      logBaseRx(packet, event.rssi, event.mac);
    } else {
      logBaseRx(packet, event.rssi, event.mac);
    }
    return;
  }

  if (packet.senderId == DEVICE_PROBE) {
    acceptProbe(event.mac);
    if (!ensurePeer(event.mac)) return;
    if (packet.type == HELLO) {
      sendProbeStatus(event.mac, packet.sequence);
      return;
    }
    if (packet.type == PING && packet.originId == DEVICE_PROBE && packet.originSequence != 0) {
      updateSequence(probeStats, packet.originSequence);
      updateRssi(probeStats, event.rssi);
      lastProbePingMs = millis();
      logProbeRx(packet, event.rssi, event.mac);
      // Direct reply proves PROBE -> GATE -> PROBE independently of BASE.
      sendProbeStatus(event.mac, packet.sequence, packet.originSequence);
      if (!baseKnown) {
        Serial.printf("DROP|ms=%lu|node=PROBE|reason=NO_BASE_ROUTE|seq=%lu\n",
                      (unsigned long)millis(), (unsigned long)packet.originSequence);
        return;
      }
      EspNowPacket relay = makePacket(RELAY_PING, 0, packet.metricMs, packet.flags);
      relay.originId = DEVICE_PROBE;
      relay.originSequence = packet.originSequence;
      relay.timestampMs = packet.timestampMs;
      relay.hopCount = 1;
      relay.relayRssi = event.rssi;
      sendRawPacket(baseMac, "BASE", relay);
      return;
    }
  }

  Serial.printf("DROP|ms=%lu|reason=UNEXPECTED_ROUTE|sender=%u|type=%s\n",
                (unsigned long)millis(), packet.senderId, packetTypeName(packet.type));
}

void sendHello(uint32_t now) {
  if (!hasElapsed(now, lastHelloMs, HELLO_INTERVAL_MS)) return;
  lastHelloMs = now;
  sendPacket(BROADCAST_MAC, "BASE", HELLO);
}

void sendGateBeacon(uint32_t now) {
  if (!hasElapsed(now, lastGateBeaconMs, SEND_INTERVAL_MS)) return;
  lastGateBeaconMs = now;
  EspNowPacket beacon = makePacket(GATE_BEACON);
  if (baseOnline) beacon.flags |= 0x02;
  sendRawPacket(BROADCAST_MAC, "PROBE", beacon);
}

void sendHeartbeat(uint32_t now) {
  if (!baseKnown || !hasElapsed(now, lastHeartbeatMs, SEND_INTERVAL_MS)) return;
  lastHeartbeatMs = now;
  if (waitingForAck) ++applicationMissed;  // New heartbeat supersedes an unconfirmed old one.

  uint32_t sentSequence = 0;
  const uint8_t flags = rttPendingReport ? PACKET_FLAG_NEW_RTT : 0;
  const uint32_t metric = rttPendingReport ? lastRttMs : UINT32_MAX;
  const bool queued = sendPacket(baseMac, "BASE", HEARTBEAT, 0, metric, flags, &sentSequence);
  if (queued) {
    pendingSequence = sentSequence;
    pendingSentMs = millis();
    waitingForAck = true;
    ++applicationSent;
    rttPendingReport = false;
  } else {
    waitingForAck = false;
    ++applicationMissed;
  }
}

void updateLink(uint32_t now) {
  if (baseOnline && hasElapsed(now, lastValidAckMs, LINK_TIMEOUT_MS)) {
    baseOnline = false;
    Serial.printf("EVENT|ms=%lu|node=BASE|state=OFFLINE\n", (unsigned long)now);
  }
}

void printStatus(uint32_t now) {
  char macText[18] = "N/A";
  char rssiText[8];
  if (baseKnown) macToText(baseMac, macText, sizeof(macText));
  rssiToText(baseStats.rssiCurrent, rssiText, sizeof(rssiText));
  const uint32_t lastAck = lastValidAckMs == 0 ? 0 : (uint32_t)(now - lastValidAckMs);
  Serial.printf("STAT|ms=%lu|node=BASE|mac=%s|online=%s|rx=%lu|lost=%lu|dup=%lu|ooo=%lu|pdr=%.2f|rssi=%s|last_ack=%lu",
                (unsigned long)now, macText, baseOnline ? "YES" : "NO",
                (unsigned long)baseStats.received, (unsigned long)baseStats.estimatedLost,
                (unsigned long)baseStats.duplicates, (unsigned long)baseStats.outOfOrder,
                pdr(baseStats), rssiText, (unsigned long)lastAck);
  if (baseStats.rttSamples > 0) {
    Serial.printf("|rtt=%lu|rtt_avg=%.1f", (unsigned long)baseStats.rttCurrent,
                  (double)baseStats.rttSum / baseStats.rttSamples);
  } else Serial.print("|rtt=N/A|rtt_avg=N/A");
  Serial.printf("|app_tx=%lu|app_ack=%lu|app_missed=%lu|queue_drop=%lu\n",
                (unsigned long)applicationSent, (unsigned long)applicationConfirmed,
                (unsigned long)applicationMissed, (unsigned long)droppedRxEvents);

  char probeMacText[18] = "N/A";
  char probeRssiText[8];
  if (probeKnown) macToText(probeMac, probeMacText, sizeof(probeMacText));
  rssiToText(probeStats.rssiCurrent, probeRssiText, sizeof(probeRssiText));
  const uint32_t probeLastSeen = lastProbePingMs == 0 ? 0 : (uint32_t)(now - lastProbePingMs);
  Serial.printf("STAT|ms=%lu|node=PROBE|hop=PROBE_GATE|mac=%s|online=%s|rx=%lu|lost=%lu|dup=%lu|ooo=%lu|pdr=%.2f|rssi=%s|last_seen=%lu\n",
                (unsigned long)now, probeMacText,
                probeKnown && probeLastSeen < LINK_TIMEOUT_MS ? "YES" : "NO",
                (unsigned long)probeStats.received, (unsigned long)probeStats.estimatedLost,
                (unsigned long)probeStats.duplicates, (unsigned long)probeStats.outOfOrder,
                pdr(probeStats), probeRssiText, (unsigned long)probeLastSeen);
}

bool startEspNow() {
  WiFi.mode(WIFI_STA);
  const esp_err_t ps = esp_wifi_set_ps(WIFI_PS_NONE);
  Serial.printf("CFG|wifi_power_save=%s\n", ps == ESP_OK ? "OFF" : "UNAVAILABLE");
  esp_err_t result = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (result != ESP_OK) {
    Serial.printf("ERR|where=SET_CHANNEL|code=%d\n", (int)result);
    return false;
  }
  result = esp_now_init();
  if (result != ESP_OK) {
    Serial.printf("ERR|where=ESPNOW_INIT|code=%d\n", (int)result);
    return false;
  }
  if (esp_now_register_recv_cb(onDataRecv) != ESP_OK || esp_now_register_send_cb(onDataSent) != ESP_OK) {
    Serial.println("ERR|where=REGISTER_CALLBACK");
    return false;
  }
  return ensurePeer(BROADCAST_MAC);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  baseStats.rssiCurrent = RSSI_NOT_AVAILABLE;
  probeStats.rssiCurrent = RSSI_NOT_AVAILABLE;
  rxQueue = xQueueCreate(24, sizeof(RxEvent));
  if (rxQueue == nullptr) {
    Serial.println("ERR|where=RX_QUEUE");
    return;
  }
  Serial.printf("BOOT|node=GATE|channel=%d|protocol=%d|idf=%d.%d.%d|rx_rssi=%s\n",
                ESPNOW_CHANNEL, PROTOCOL_VERSION, ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR,
                ESP_IDF_VERSION_PATCH, ESPNOW_RX_INFO_CALLBACK ? "AVAILABLE" : "N/A");
  espNowReady = startEspNow();
  if (!espNowReady) return;
  lastHelloMs = millis() - HELLO_INTERVAL_MS;
}

void loop() {
  if (!espNowReady) return;
  RxEvent event;
  while (rxQueue != nullptr && xQueueReceive(rxQueue, &event, 0) == pdPASS) processRx(event);
  const uint32_t now = millis();
  sendHello(now);
  sendGateBeacon(now);
  sendHeartbeat(now);
  updateLink(now);
  if (hasElapsed(now, lastStatusMs, 5000UL)) {
    lastStatusMs = now;
    printStatus(now);
  }
}
