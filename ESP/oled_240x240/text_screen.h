#ifndef TEXT_SCREEN_H
#define TEXT_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*text_draw_line_cb)(int index, const char* line);

#ifdef __cplusplus
}
#endif

// Header-only implementation (C/C++): dados internos + funcoes inline
static const char* TEXT_SCREEN_LINES[] = {
  "Teste de texto 1",
  "Teste de texto 2",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "abcdefghijklmnopqrstuvwxyz",
  "0123456789 !@#$%&*()_+[]{}",
  "Acentos: á à ã â ç é ê í ó ô õ ú ü",
  "Linha longa para ver corte/overflow: Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
  "GC9A01 + ESP32 + TFT_eSPI",
  "Linha 9",
  "Linha 10",
  "Linha 11",
  "Linha 12",
  "Linha 13",
  "Linha 14",
  "Linha 15",
};

static const int TEXT_SCREEN_NUM_LINES = (int)(sizeof(TEXT_SCREEN_LINES)/sizeof(TEXT_SCREEN_LINES[0]));

static inline int text_screen_line_count(void) {
  return TEXT_SCREEN_NUM_LINES;
}

static inline const char* text_screen_get_line(int idx) {
  if (idx < 0 || idx >= TEXT_SCREEN_NUM_LINES) return 0;
  return TEXT_SCREEN_LINES[idx];
}

static inline void text_screen_iterate(text_draw_line_cb cb) {
  if (!cb) return;
  for (int i = 0; i < TEXT_SCREEN_NUM_LINES; i++) {
    cb(i, TEXT_SCREEN_LINES[i]);
  }
}

#endif // TEXT_SCREEN_H


