#pragma once

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
