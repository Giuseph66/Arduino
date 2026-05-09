// --- Driver ---
#define USER_SETUP_LOADED
#define GC9A01_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// --- Pinos ESP32 ---
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS    5
#define TFT_DC   16
#define TFT_RST   4
#define TFT_BL   15     // use se quiser PWM; senão, deixe BLK no 3V3

// --- Frequências (seguras p/ começar) ---
#define SPI_FREQUENCY 27000000
//#define SPI_FREQUENCY 40000000   // dá para subir depois

// --- Fontes da TFT_eSPI ---
#define LOAD_GLCD   // fonte 1 (8x8)
#define LOAD_FONT2  // 16px
#define LOAD_FONT4  // 26px
