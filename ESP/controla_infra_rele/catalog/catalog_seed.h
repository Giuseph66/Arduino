#pragma once

#include <Arduino.h>

struct CatalogDeviceType {
  const char* id;
  const char* label;
  bool stateBased;
};

struct CatalogBrand {
  const char* id;
  const char* label;
  const char* deviceTypes;
};

struct CatalogCandidate {
  const char* id;
  const char* deviceType;
  const char* brand;
  const char* model;
  const char* commandKey;
  const char* label;
  const char* type;
  const char* protocol;
  uint64_t value;
  uint16_t bits;
  uint16_t carrier;
  uint8_t repeat;
};

struct TestEvent {
  int candidateIndex;
  unsigned long sentAt;
};

struct TestSession {
  bool active;
  bool autoMode;
  String id;
  String deviceType;
  String brand;
  uint16_t cursor;
  uint16_t sentCount;
  uint16_t delayMs;
  unsigned long nextSendAt;
  TestEvent events[5];
  uint8_t eventCount;
};

const CatalogDeviceType catalogDeviceTypes[] = {
  {"tv", "Televisao", false},
  {"air_conditioner", "Ar-condicionado", true},
  {"projector", "Projetor", false},
  {"soundbar", "Soundbar", false},
  {"receiver", "Receiver/Home Theater", false},
  {"set_top_box", "Decoder/TV Box", false},
};

const CatalogBrand catalogBrands[] = {
  {"samsung", "Samsung", "tv,soundbar,air_conditioner"},
  {"lg", "LG", "tv,soundbar,air_conditioner,projector"},
  {"sony", "Sony", "tv,audio,projector"},
  {"panasonic", "Panasonic", "tv,soundbar,projector,air_conditioner"},
  {"tcl", "TCL", "tv,air_conditioner"},
  {"gree", "Gree", "air_conditioner"},
  {"midea", "Midea/Springer Midea", "air_conditioner"},
  {"daikin", "Daikin", "air_conditioner"},
  {"fujitsu", "Fujitsu", "air_conditioner"},
  {"epson", "Epson", "projector"},
  {"acer", "Acer", "projector"},
  {"sanyo", "Sanyo", "tv,projector"},
  {"yamaha", "Yamaha", "receiver"},
  {"jbl", "JBL", "soundbar,audio"},
  {"apple", "Apple", "set_top_box"},
};

const CatalogCandidate catalogCandidates[] = {
  {"cmd_samsung_aa59_volume_up", "tv", "Samsung", "AA59", "volume_up", "Volume +", "decoded", "SAMSUNG", 0xE0E0E01FULL, 32, 38, 1},
  {"cmd_samsung_aa59_power_toggle", "tv", "Samsung", "AA59", "power_toggle", "Power", "decoded", "SAMSUNG", 0xE0E040BFULL, 32, 38, 1},
  {"cmd_lg_55uh8509_input", "tv", "LG", "55UH8509", "input", "Input", "decoded", "NEC", 0x20DFD02FULL, 32, 38, 1},
  {"cmd_lg_55uh8509_power", "tv", "LG", "55UH8509", "power_toggle", "Power", "decoded", "NEC", 0x20DF10EFULL, 32, 38, 1},
  {"cmd_sony_kdl_ex540_input", "tv", "Sony", "KDL-EX540", "input", "Input", "decoded", "SONY", 0xA50ULL, 12, 40, 3},
  {"cmd_sony_kdl_ex540_power_on", "tv", "Sony", "KDL-EX540", "power_on", "Power On", "decoded", "SONY", 0x750ULL, 12, 40, 3},
  {"cmd_panasonic_tx65fxw784_power", "tv", "Panasonic", "TX-65FXW784", "power_toggle", "Power", "decoded", "PANASONIC", 0x40040100BCBDULL, 48, 37, 1},
  {"cmd_acer_k132_power", "projector", "Acer", "K132", "power_toggle", "Power", "decoded", "NEC", 0x10C8E11EULL, 32, 38, 1},
  {"cmd_sanyo_plvz4_power_toggle", "projector", "Sanyo", "PLV-Z4", "power_toggle", "Power", "decoded", "NEC", 0xCC0000FFULL, 32, 38, 1},
  {"cmd_panasonic_scall70t_power", "soundbar", "Panasonic", "SC-ALL70T", "power_toggle", "Power", "decoded", "PANASONIC", 0x40040500BCB9ULL, 48, 37, 1},
  {"cmd_jbl_onair24g_volume_up", "soundbar", "JBL", "On Air 2.4G", "volume_up", "Volume +", "decoded", "NEC", 0x538521DEULL, 32, 38, 1},
  {"cmd_yamaha_rxv1900_power_on", "receiver", "Yamaha", "RX-V1900", "power_on", "Power On", "decoded", "NEC", 0x7E817E81ULL, 32, 38, 1},
  {"cmd_appletv_gen4_menu", "set_top_box", "Apple", "Apple TV Gen4", "menu", "Menu", "decoded", "NEC", 0x77E1C080ULL, 32, 38, 1},
  {"ac_daikin_cool_24", "air_conditioner", "Daikin", "ARC433 family", "power_on_cool_24_auto", "Cool 24 auto", "climate_state", "DAIKIN_AC", 0, 0, 38, 1},
  {"ac_gree_cool_24", "air_conditioner", "Gree", "Generic", "power_on_cool_24_auto", "Cool 24 auto", "climate_state", "GREE_AC", 0, 0, 38, 1},
  {"ac_midea_cool_24", "air_conditioner", "Midea/Springer Midea", "Generic", "power_on_cool_24_auto", "Cool 24 auto", "climate_state", "MIDEA_AC", 0, 0, 38, 1},
  {"ac_fujitsu_cool_24", "air_conditioner", "Fujitsu", "Generic", "power_on_cool_24_auto", "Cool 24 auto", "climate_state", "FUJITSU_AC", 0, 0, 38, 1},
  {"ac_samsung_cool_24", "air_conditioner", "Samsung", "Generic", "power_on_cool_24_auto", "Cool 24 auto", "climate_state", "SAMSUNG_AC", 0, 0, 38, 1},
};

TestSession testSession;
uint32_t nextSessionId = 1;

