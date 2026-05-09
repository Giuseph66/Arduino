#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#define TFT_BL 21

constexpr uint8_t kTouchCsPin = 33;
constexpr uint8_t kTouchMosiPin = 32;
constexpr uint8_t kTouchMisoPin = 39;
constexpr uint8_t kTouchClkPin = 25;
constexpr uint16_t kTouchThreshold = 450;
constexpr uint16_t kDefaultTouchCal[5] = {200, 3600, 200, 3600, 0};
constexpr int16_t kToolbarH = 42;
constexpr int16_t kCanvasY = kToolbarH + 2;

struct Swatch {
  uint16_t color;
  int16_t x;
};

TFT_eSPI tft = TFT_eSPI(320, 240);
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(kTouchCsPin);
Preferences prefs;

uint16_t calData[5] = {200, 3600, 200, 3600, 0};
Swatch swatches[] = {
    {TFT_WHITE, 8},
    {TFT_RED, 40},
    {TFT_ORANGE, 72},
    {TFT_YELLOW, 104},
    {TFT_GREEN, 136},
    {TFT_CYAN, 168},
    {TFT_BLUE, 200},
    {TFT_MAGENTA, 232},
};

uint16_t activeColor = TFT_WHITE;
uint8_t brushSize = 4;
bool touchDown = false;
int16_t lastDrawX = -1;
int16_t lastDrawY = -1;

bool mapTouchPoint(const TS_Point& p, uint16_t* x, uint16_t* y) {
  if (p.z < kTouchThreshold) {
    return false;
  }

  int32_t xx = 0;
  int32_t yy = 0;
  if (calData[4] & 0x01) {
    xx = (static_cast<int32_t>(p.y) - calData[0]) * tft.width() / calData[1];
    yy = (static_cast<int32_t>(p.x) - calData[2]) * tft.height() / calData[3];
  } else {
    xx = (static_cast<int32_t>(p.x) - calData[0]) * tft.width() / calData[1];
    yy = (static_cast<int32_t>(p.y) - calData[2]) * tft.height() / calData[3];
  }

  if (calData[4] & 0x02) {
    xx = (tft.width() - 1) - xx;
  }
  if (calData[4] & 0x04) {
    yy = (tft.height() - 1) - yy;
  }

  if (xx < 0 || yy < 0 || xx >= tft.width() || yy >= tft.height()) {
    return false;
  }

  *x = static_cast<uint16_t>(xx);
  *y = static_cast<uint16_t>(yy);
  return true;
}

void drawToolbar() {
  tft.fillRect(0, 0, tft.width(), kToolbarH, TFT_DARKGREY);
  tft.drawFastHLine(0, kToolbarH, tft.width(), TFT_NAVY);

  for (uint8_t i = 0; i < (sizeof(swatches) / sizeof(swatches[0])); ++i) {
    const bool selected = swatches[i].color == activeColor;
    tft.fillRoundRect(swatches[i].x, 7, 24, 24, 6, swatches[i].color);
    tft.drawRoundRect(swatches[i].x - 1, 6, 26, 26, 7, selected ? TFT_WHITE : TFT_BLACK);
  }

  const int16_t sizeX = 264;
  tft.fillRoundRect(sizeX, 7, 18, 24, 6, TFT_BLACK);
  tft.fillRoundRect(sizeX + 22, 7, 18, 24, 6, TFT_BLACK);
  tft.fillRoundRect(sizeX + 44, 7, 18, 24, 6, TFT_BLACK);
  tft.fillCircle(sizeX + 9, 19, 2, brushSize == 2 ? TFT_YELLOW : TFT_LIGHTGREY);
  tft.fillCircle(sizeX + 31, 19, 4, brushSize == 4 ? TFT_YELLOW : TFT_LIGHTGREY);
  tft.fillCircle(sizeX + 53, 19, 6, brushSize == 7 ? TFT_YELLOW : TFT_LIGHTGREY);
  tft.drawRoundRect(sizeX, 7, 18, 24, 6, brushSize == 2 ? TFT_YELLOW : TFT_DARKGREY);
  tft.drawRoundRect(sizeX + 22, 7, 18, 24, 6, brushSize == 4 ? TFT_YELLOW : TFT_DARKGREY);
  tft.drawRoundRect(sizeX + 44, 7, 18, 24, 6, brushSize == 7 ? TFT_YELLOW : TFT_DARKGREY);

  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("CLR", tft.width() - 8, 34, 2);
  tft.setTextDatum(TL_DATUM);
}

void clearCanvas() {
  tft.fillRect(0, kCanvasY, tft.width(), tft.height() - kCanvasY, TFT_BLACK);
  tft.drawRect(0, kCanvasY, tft.width(), tft.height() - kCanvasY, TFT_DARKGREY);
}

void drawBrushPreview() {
  tft.fillRect(0, kToolbarH + 3, 44, 28, TFT_BLACK);
  tft.drawRoundRect(4, kToolbarH + 7, 34, 20, 6, TFT_DARKGREY);
  tft.fillCircle(21, kToolbarH + 17, brushSize, activeColor);
}

void handleToolbarTap(uint16_t x, uint16_t y) {
  if (y > kToolbarH) {
    return;
  }

  for (uint8_t i = 0; i < (sizeof(swatches) / sizeof(swatches[0])); ++i) {
    if (x >= swatches[i].x && x < (swatches[i].x + 24)) {
      activeColor = swatches[i].color;
      drawToolbar();
      drawBrushPreview();
      return;
    }
  }

  if (x >= 264 && x < 282) {
    brushSize = 2;
    drawToolbar();
    drawBrushPreview();
    return;
  }
  if (x >= 286 && x < 304) {
    brushSize = 4;
    drawToolbar();
    drawBrushPreview();
    return;
  }
  if (x >= 308 && x < 326) {
    brushSize = 7;
    drawToolbar();
    drawBrushPreview();
    return;
  }

  if (x >= 286 && y >= 28) {
    clearCanvas();
    drawBrushPreview();
  }
}

void drawStroke(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  if (x0 < 0 || y0 < 0) {
    tft.fillCircle(x1, y1, brushSize, activeColor);
    return;
  }

  tft.drawLine(x0, y0, x1, y1, activeColor);
  tft.fillCircle(x1, y1, brushSize, activeColor);
  tft.fillCircle(x0, y0, brushSize, activeColor);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(2);

  touchSPI.begin(kTouchClkPin, kTouchMisoPin, kTouchMosiPin, kTouchCsPin);
  touch.begin(touchSPI);
  touch.setRotation(1);

  prefs.begin("tela_touch", false);
  if (prefs.getBytesLength("touch_v2") == sizeof(calData)) {
    prefs.getBytes("touch_v2", calData, sizeof(calData));
  } else {
    memcpy(calData, kDefaultTouchCal, sizeof(calData));
  }

  drawToolbar();
  clearCanvas();
  drawBrushPreview();

  Serial.println("Paint app ready");
  Serial.printf("rotation=%d width=%d height=%d\n", 2, tft.width(), tft.height());
}

void loop() {
  if (!touch.touched()) {
    touchDown = false;
    lastDrawX = -1;
    lastDrawY = -1;
    delay(8);
    return;
  }

  const TS_Point p = touch.getPoint();
  uint16_t x = 0;
  uint16_t y = 0;
  if (!mapTouchPoint(p, &x, &y)) {
    delay(8);
    return;
  }

  if (y <= kToolbarH) {
    if (!touchDown) {
      handleToolbarTap(x, y);
    }
    touchDown = true;
    lastDrawX = -1;
    lastDrawY = -1;
    delay(20);
    return;
  }

  if (y >= kCanvasY) {
    drawStroke(lastDrawX, lastDrawY, x, y);
    lastDrawX = x;
    lastDrawY = y;
    touchDown = true;
  }

  delay(8);
}
