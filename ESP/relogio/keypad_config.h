#ifndef KEYPAD_CONFIG_H
#define KEYPAD_CONFIG_H

// Configuração dos pinos da matriz 4x4
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4

// Mapeamento dos pinos (ajuste conforme sua conexão)
#define ROW_PINS {27, 14, 12, 13}  // R1, R2, R3, R4
#define COL_PINS {25, 26, 33, 32}  // C1, C2, C3, C4 (C1/C2 invertidos)

// Caracteres das teclas
#define KEYPAD_KEYS {{'1','2','3','4'}, \
                     {'5','6','7','8'}, \
                     {'9','A','B','C'}, \
                     {'D','E','F','G'}}

// Nomes das teclas para identificação
#define BUTTON_NAMES {"S1", "S2", "S3", "S4", \
                      "S5", "S6", "S7", "S8", \
                      "S9", "S10", "S11", "S12", \
                      "S13", "S14", "S15", "S16"}

// Configurações de tempo
#define LONG_PRESS_THRESHOLD 2000       // 2 segundos para clique longo

// Ações dos botões (definidas em executarAcaoBotao)
enum ButtonActions {
  ACTION_INFO_SYSTEM,      // S1 - Informações do sistema
  ACTION_STATUS_WIFI,      // S1 - Status WiFi
  ACTION_INFO_COMPLETE,    // S2 - Informações completas
  ACTION_STATUS_AUDIO,     // S2 - Status do áudio
  ACTION_STATUS_BUTTONS,   // S3 - Status dos botões
  ACTION_AUDIO_START,      // S15 - Iniciar áudio
  ACTION_AUDIO_STOP,       // S16 - Parar áudio
  ACTION_CUSTOM            // S4-S14 - Ações personalizáveis
};

#endif // KEYPAD_CONFIG_H 