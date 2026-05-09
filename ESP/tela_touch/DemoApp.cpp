#include "DemoApp.h"

#include <math.h>

namespace {

constexpr uint32_t kAnimIntervalMs = 90;
constexpr uint8_t kSceneCount = 6;
constexpr uint8_t kBootButtonPin = 0;
constexpr uint8_t kTouchCsPin = 33;
constexpr uint8_t kTouchMosiPin = 32;
constexpr uint8_t kTouchMisoPin = 39;
constexpr uint8_t kTouchClkPin = 25;
constexpr uint16_t kTouchThreshold = 450;
constexpr int16_t kTouchInset = 28;
constexpr const char* kPrefsNamespace = "tela_touch";
constexpr const char* kTouchCalKey = "touch_v2";
constexpr uint16_t kDefaultTouchCal[5] = {200, 3600, 200, 3600, 0};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr Rect kPrevButtonRect = {12, 208, 74, 24};
constexpr Rect kNextButtonRect = {234, 208, 74, 24};
constexpr Rect kWidgetArea = {10, 64, 300, 138};

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

struct Theme {
  uint16_t bg;
  uint16_t panel;
  uint16_t panel_alt;
  uint16_t text;
  uint16_t muted;
  uint16_t aqua;
  uint16_t blue;
  uint16_t orange;
  uint16_t yellow;
  uint16_t red;
  uint16_t green;
};

const Theme kTheme = {
    rgb565(5, 10, 20),
    rgb565(16, 23, 42),
    rgb565(27, 36, 60),
    TFT_WHITE,
    TFT_LIGHTGREY,
    rgb565(0, 210, 180),
    rgb565(0, 125, 255),
    rgb565(255, 110, 65),
    rgb565(255, 205, 0),
    rgb565(255, 72, 92),
    rgb565(80, 220, 120),
};

struct SceneMeta {
  const char* title;
  const char* subtitle;
};

const SceneMeta kSceneMeta[kSceneCount] = {
    {"Overview", "System summary and quick stats"},
    {"Control Panel", "Touch PREV and NEXT to move between scenes"},
    {"Signals", "Live traces rendered only inside the graph area"},
    {"Motion Lab", "Particle field updated in a local sprite"},
    {"Sensors", "Dense telemetry cards with low-cost animation"},
    {"Console", "Rolling event feed with status pulses"},
};

float wave01(uint32_t ms, float speed, float phase = 0.0f) {
  return 0.5f + 0.5f * sinf(ms * speed + phase);
}

bool contains(const Rect& rect, uint16_t x, uint16_t y) {
  return x >= rect.x && x < (rect.x + rect.w) && y >= rect.y && y < (rect.y + rect.h);
}

void drawCalibrationTarget(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color) {
  const int16_t arm = 10;
  tft.drawFastHLine(x - arm, y, arm * 2 + 1, color);
  tft.drawFastVLine(x, y - arm, arm * 2 + 1, color);
  tft.drawCircle(x, y, 12, color);
  tft.drawCircle(x, y, 4, color);
}

bool waitForRelease(XPT2046_Touchscreen& touch, uint32_t timeout_ms) {
  const uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    if (!touch.touched()) {
      return true;
    }
    delay(5);
  }
  return false;
}

bool sampleRawTouch(XPT2046_Touchscreen& touch, uint16_t* x, uint16_t* y, uint32_t timeout_ms) {
  const uint32_t start = millis();
  uint8_t press_count = 0;
  uint32_t sx = 0;
  uint32_t sy = 0;
  uint8_t samples = 0;

  while ((millis() - start) < timeout_ms) {
    if (touch.touched()) {
      if (press_count < 4) {
        ++press_count;
        delay(12);
        continue;
      }

      const TS_Point p = touch.getPoint();
      if (p.z < kTouchThreshold) {
        delay(8);
        continue;
      }
      const uint16_t rx = static_cast<uint16_t>(p.x);
      const uint16_t ry = static_cast<uint16_t>(p.y);
      sx += rx;
      sy += ry;
      ++samples;
      if (samples >= 16) {
        *x = static_cast<uint16_t>(sx / samples);
        *y = static_cast<uint16_t>(sy / samples);
        waitForRelease(touch, 1200);
        return true;
      }
    } else {
      if (press_count > 0 && samples == 0) {
        press_count = 0;
      }
    }
    delay(8);
  }

  return false;
}

bool validateCalibrationSample(uint8_t index, const uint16_t* raw, uint16_t rx, uint16_t ry) {
  constexpr int32_t kNearAxisMax = 700;
  constexpr int32_t kFarAxisMin = 1200;

  if (index == 0) {
    return true;
  }

  const int32_t base_x = raw[0];
  const int32_t base_y = raw[1];
  const int32_t dx01 = static_cast<int32_t>(raw[2]) - base_x;
  const int32_t dy01 = static_cast<int32_t>(raw[3]) - base_y;

  if (index == 1) {
    const int32_t dx = static_cast<int32_t>(rx) - base_x;
    const int32_t dy = static_cast<int32_t>(ry) - base_y;
    const bool mostly_one_axis =
        (abs(dx) <= kNearAxisMax && abs(dy) >= kFarAxisMin) ||
        (abs(dy) <= kNearAxisMax && abs(dx) >= kFarAxisMin);
    return mostly_one_axis;
  }

  const bool vertical_is_x = abs(dx01) > abs(dy01);
  const int32_t vertical_delta = vertical_is_x
                                     ? static_cast<int32_t>(rx) - base_x
                                     : static_cast<int32_t>(ry) - base_y;
  const int32_t horizontal_delta = vertical_is_x
                                       ? static_cast<int32_t>(ry) - base_y
                                       : static_cast<int32_t>(rx) - base_x;

  if (index == 2) {
    return abs(vertical_delta) <= kNearAxisMax && abs(horizontal_delta) >= kFarAxisMin;
  }

  return abs(vertical_delta) >= kFarAxisMin && abs(horizontal_delta) >= kFarAxisMin;
}

void storeTouchCalibration(uint16_t* cal, uint16_t xl, uint16_t xr, uint16_t yt, uint16_t yb,
                           bool rotate, bool invert_x, bool invert_y,
                           int16_t width, int16_t height, int16_t inset) {
  const uint32_t usable_w = width - (inset * 2);
  const uint32_t usable_h = height - (inset * 2);
  uint32_t span_x = static_cast<uint32_t>(abs(static_cast<int32_t>(xr) - static_cast<int32_t>(xl)));
  uint32_t span_y = static_cast<uint32_t>(abs(static_cast<int32_t>(yb) - static_cast<int32_t>(yt)));

  if (usable_w == 0 || usable_h == 0 || span_x == 0 || span_y == 0) {
    return;
  }

  span_x = (span_x * width) / usable_w;
  span_y = (span_y * height) / usable_h;

  uint32_t x0 = xl;
  uint32_t y0 = yt;
  const uint32_t x_pad = (span_x * inset) / width;
  const uint32_t y_pad = (span_y * inset) / height;
  x0 = (x0 > x_pad) ? (x0 - x_pad) : 1;
  y0 = (y0 > y_pad) ? (y0 - y_pad) : 1;

  cal[0] = static_cast<uint16_t>(x0 == 0 ? 1 : x0);
  cal[1] = static_cast<uint16_t>(span_x == 0 ? 1 : span_x);
  cal[2] = static_cast<uint16_t>(y0 == 0 ? 1 : y0);
  cal[3] = static_cast<uint16_t>(span_y == 0 ? 1 : span_y);
  cal[4] = (rotate ? 1 : 0) | (invert_x ? 2 : 0) | (invert_y ? 4 : 0);
}

bool calibrateTouchInset(TFT_eSPI& tft, XPT2046_Touchscreen& touch, uint16_t* cal) {
  const int16_t w = tft.width();
  const int16_t h = tft.height();
  const int16_t ix = kTouchInset;
  const int16_t iy = kTouchInset;
  const int16_t tx[4] = {ix, ix, static_cast<int16_t>(w - 1 - ix), static_cast<int16_t>(w - 1 - ix)};
  const int16_t ty[4] = {iy, static_cast<int16_t>(h - 1 - iy), iy, static_cast<int16_t>(h - 1 - iy)};
  uint16_t raw[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Touch calibration", w / 2, 20, 4);
  tft.drawString("Tap and hold each target", w / 2, 46, 2);
  tft.drawString("Targets are offset from the edges", w / 2, 64, 2);
  tft.setTextDatum(TL_DATUM);

  for (uint8_t i = 0; i < 4; ++i) {
    tft.fillRect(0, 86, w, h - 86, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(String(i + 1) + "/4", 12, 92, 2);
    tft.drawString("Wait for green confirm", 12, 112, 2);
    drawCalibrationTarget(tft, tx[i], ty[i], TFT_MAGENTA);

    bool accepted = false;
    while (!accepted) {
      if (!waitForRelease(touch, 600)) {
        delay(50);
      }

      uint16_t rx = 0;
      uint16_t ry = 0;
      if (!sampleRawTouch(touch, &rx, &ry, 8000)) {
        return false;
      }

      if (!validateCalibrationSample(i, raw, rx, ry)) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.fillRect(12, 132, 220, 18, TFT_BLACK);
        tft.drawString("Touch the highlighted target", 12, 132, 2);
        delay(700);
        tft.fillRect(12, 132, 220, 18, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        continue;
      }

      raw[i * 2] = rx;
      raw[i * 2 + 1] = ry;
      tft.fillCircle(tx[i], ty[i], 6, TFT_GREEN);
      delay(180);
      accepted = true;
    }
  }

  bool rotate = false;
  uint16_t x0 = 0;
  uint16_t x1 = 0;
  uint16_t y0 = 0;
  uint16_t y1 = 0;

  if (abs(static_cast<int32_t>(raw[0]) - static_cast<int32_t>(raw[4])) >
      abs(static_cast<int32_t>(raw[1]) - static_cast<int32_t>(raw[5]))) {
    rotate = true;
    x0 = (raw[1] + raw[3]) / 2;
    x1 = (raw[5] + raw[7]) / 2;
    y0 = (raw[0] + raw[4]) / 2;
    y1 = (raw[2] + raw[6]) / 2;
  } else {
    x0 = (raw[0] + raw[2]) / 2;
    x1 = (raw[4] + raw[6]) / 2;
    y0 = (raw[1] + raw[5]) / 2;
    y1 = (raw[3] + raw[7]) / 2;
  }

  bool invert_x = false;
  if (x0 > x1) {
    const uint16_t tmp = x0;
    x0 = x1;
    x1 = tmp;
    invert_x = true;
  }

  bool invert_y = false;
  if (y0 > y1) {
    const uint16_t tmp = y0;
    y0 = y1;
    y1 = tmp;
    invert_y = true;
  }

  storeTouchCalibration(cal, x0, x1, y0, y1, rotate, invert_x, invert_y, w, h, kTouchInset);
  return true;
}

void clearSpriteRegion(TFT_eSprite& spr, int16_t w, int16_t h, uint16_t color) {
  spr.fillRect(0, 0, w, h, color);
}

void header(TFT_eSPI& tft, const char* title, uint8_t index) {
  tft.fillRect(0, 0, tft.width(), 28, kTheme.panel_alt);
  tft.drawFastHLine(0, 28, tft.width(), kTheme.aqua);
  tft.setTextColor(kTheme.text, kTheme.panel_alt);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("ESP32 Display Module", 10, 7, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(String(index + 1) + "/" + String(kSceneCount), tft.width() - 10, 7, 2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kTheme.text, kTheme.bg);
  tft.drawString(title, 12, 38, 4);
}

void card(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border) {
  tft.fillRoundRect(x, y, w, h, 10, kTheme.panel);
  tft.drawRoundRect(x, y, w, h, 10, border);
}

void navButton(TFT_eSPI& canvas, const Rect& rect, const char* label, uint16_t accent, bool pressed) {
  const uint16_t fill = pressed ? accent : kTheme.panel_alt;
  const uint16_t text = pressed ? kTheme.bg : kTheme.text;
  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 10, fill);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 10, accent);
  canvas.setTextColor(text, fill);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString(label, rect.x + rect.w / 2, rect.y + rect.h / 2, 2);
  canvas.setTextDatum(TL_DATUM);
}

void drawDialSprite(TFT_eSprite& spr, int16_t w, int16_t h, float value,
                    uint16_t accent, const String& label) {
  const int16_t cx = w / 2;
  const int16_t cy = h / 2 - 6;
  int16_t r = (w / 2) - 16;
  if (((h / 2) - 20) < r) {
    r = (h / 2) - 20;
  }
  if (r < 24) {
    r = 24;
  }

  const float clamped = constrain(value, 0.0f, 1.0f);
  const float a0 = -2.3f;
  const float a1 = 0.35f;
  const float needle = a0 + (a1 - a0) * clamped;

  clearSpriteRegion(spr, w, h, kTheme.panel);
  spr.drawRoundRect(0, 0, w, h, 10, accent);
  spr.drawCircle(cx, cy, r, kTheme.muted);
  spr.drawCircle(cx, cy, r - 1, kTheme.panel_alt);
  spr.fillCircle(cx, cy, r - 12, kTheme.panel_alt);

  for (int i = 0; i <= 10; ++i) {
    const float a = a0 + (a1 - a0) * (i / 10.0f);
    const int16_t x0 = static_cast<int16_t>(cx + cosf(a) * (r - 8));
    const int16_t y0 = static_cast<int16_t>(cy + sinf(a) * (r - 8));
    const int16_t x1 = static_cast<int16_t>(cx + cosf(a) * (r - 1));
    const int16_t y1 = static_cast<int16_t>(cy + sinf(a) * (r - 1));
    spr.drawLine(x0, y0, x1, y1, kTheme.muted);
  }

  const int16_t nx = static_cast<int16_t>(cx + cosf(needle) * (r - 16));
  const int16_t ny = static_cast<int16_t>(cy + sinf(needle) * (r - 16));
  spr.drawLine(cx, cy, nx, ny, accent);
  spr.fillCircle(cx, cy, 4, accent);

  spr.setTextColor(kTheme.text, kTheme.panel_alt);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(String(static_cast<int>(clamped * 100)) + "%", cx, cy + 2, (r >= 34) ? 4 : 2);
  spr.drawString(label, cx, h - 18, 2);
  spr.setTextDatum(TL_DATUM);
}

void drawBarsSprite(TFT_eSprite& spr, int16_t w, int16_t h, float a, float b, float c) {
  clearSpriteRegion(spr, w, h, kTheme.panel);
  spr.drawRoundRect(0, 0, w, h, 10, kTheme.orange);
  const bool compact = h < 44;
  const int16_t top = 6;
  const int16_t row_h = (h - 12) / 3;
  const int16_t label_x = 12;
  const int16_t track_x = compact ? 46 : 62;
  const int16_t track_w = w - track_x - 14;
  int16_t bar_h = row_h - (compact ? 2 : 6);
  if (bar_h < 8) {
    bar_h = 8;
  }
  const uint8_t label_font = compact ? 1 : 2;

  const struct {
    const char* label;
    float value;
    uint16_t color;
  } bars[] = {
      {"CPU", a, kTheme.aqua},
      {"LINK", b, kTheme.blue},
      {"RAM", c, kTheme.orange},
  };

  for (int i = 0; i < 3; ++i) {
    const int16_t y = top + i * row_h + (row_h - bar_h) / 2;
    spr.setTextColor(kTheme.text, kTheme.panel);
    spr.drawString(bars[i].label, label_x, y + (compact ? 1 : 0), label_font);
    spr.drawRoundRect(track_x, y, track_w, bar_h, 5, kTheme.muted);
    spr.fillRoundRect(track_x + 1, y + 1, track_w - 2, bar_h - 2, 4, kTheme.panel_alt);
    const int16_t fill = static_cast<int16_t>((track_w - 2) * constrain(bars[i].value, 0.0f, 1.0f));
    if (fill > 0) {
      spr.fillRoundRect(track_x + 1, y + 1, fill, bar_h - 2, 4, bars[i].color);
    }
  }
}

void drawGraphSprite(TFT_eSprite& spr, int16_t w, int16_t h, uint32_t scene_ms) {
  const int16_t center = h / 2;

  clearSpriteRegion(spr, w, h, kTheme.panel);
  spr.drawRoundRect(0, 0, w, h, 10, kTheme.blue);

  for (int16_t gx = 12; gx < w; gx += 24) {
    spr.drawFastVLine(gx, 10, h - 20, kTheme.panel_alt);
  }
  for (int16_t gy = 12; gy < h; gy += 20) {
    spr.drawFastHLine(10, gy, w - 20, kTheme.panel_alt);
  }

  int16_t px = 10;
  int16_t py1 = center;
  int16_t py2 = center;
  for (int16_t i = 1; i < w - 20; ++i) {
    const float t = scene_ms * 0.004f + i * 0.09f;
    const int16_t x = 10 + i;
    const int16_t y1 = center + static_cast<int16_t>(sinf(t) * 28.0f);
    const int16_t y2 = center + static_cast<int16_t>(cosf(t * 0.7f + 1.4f) * 22.0f);
    spr.drawLine(px, py1, x, y1, kTheme.aqua);
    spr.drawLine(px, py2, x, y2, kTheme.orange);
    px = x;
    py1 = y1;
    py2 = y2;
  }
}

void drawMotionSprite(TFT_eSprite& spr, int16_t w, int16_t h, uint32_t scene_ms) {
  clearSpriteRegion(spr, w, h, kTheme.panel);
  spr.drawRoundRect(0, 0, w, h, 12, kTheme.orange);

  for (int i = 0; i < 12; ++i) {
    const float phase = scene_ms * 0.0021f + i * 0.7f;
    const int16_t x = 18 + static_cast<int16_t>((sinf(phase * 0.9f) * 0.5f + 0.5f) * (w - 36));
    const int16_t y = 18 + static_cast<int16_t>((cosf(phase * 1.2f + 0.8f) * 0.5f + 0.5f) * (h - 36));
    const int16_t r = 4 + (i % 4) * 3;
    const uint16_t color = (i % 3 == 0) ? kTheme.aqua : (i % 3 == 1) ? kTheme.blue : kTheme.orange;
    spr.fillCircle(x, y, r, color);
  }
}

void drawSensorGridSprite(TFT_eSprite& spr, int16_t w, int16_t h,
                          float temp, float hum, float power, float link) {
  clearSpriteRegion(spr, w, h, kTheme.panel);
  const int16_t gap = 8;
  const int16_t box_w = (w - gap) / 2;
  const int16_t box_h = (h - gap) / 2;

  const struct {
    const char* label;
    float value;
    uint16_t color;
    const char* unit;
  } items[] = {
      {"TEMP", temp, kTheme.red, "C"},
      {"HUM", hum, kTheme.aqua, "%"},
      {"PWR", power, kTheme.yellow, "W"},
      {"LINK", link, kTheme.green, "%"},
  };

  for (int i = 0; i < 4; ++i) {
    const int16_t x = (i % 2) * (box_w + gap);
    const int16_t y = (i / 2) * (box_h + gap);
    spr.fillRoundRect(x, y, box_w, box_h, 10, kTheme.panel_alt);
    spr.drawRoundRect(x, y, box_w, box_h, 10, items[i].color);
    spr.setTextColor(kTheme.muted, kTheme.panel_alt);
    spr.drawString(items[i].label, x + 12, y + 10, 2);
    spr.setTextColor(kTheme.text, kTheme.panel_alt);

    const int value = (i == 0) ? static_cast<int>(18 + items[i].value * 16)
                               : static_cast<int>(items[i].value * 100);
    spr.drawString(String(value) + items[i].unit, x + 12, y + 34, 4);

    const int16_t track_w = box_w - 24;
    const int16_t fill = static_cast<int16_t>(track_w * constrain(items[i].value, 0.0f, 1.0f));
    spr.drawRoundRect(x + 12, y + box_h - 20, track_w, 8, 4, kTheme.muted);
    spr.fillRoundRect(x + 13, y + box_h - 19, track_w - 2, 6, 3, kTheme.panel);
    if (fill > 0) {
      spr.fillRoundRect(x + 13, y + box_h - 19, fill, 6, 3, items[i].color);
    }
  }
}

void drawConsoleSprite(TFT_eSprite& spr, int16_t w, int16_t h, uint32_t scene_ms) {
  clearSpriteRegion(spr, w, h, kTheme.panel);
  spr.drawRoundRect(0, 0, w, h, 10, kTheme.yellow);

  const int16_t line_h = 24;
  const int16_t top = 10;
  const int active = (scene_ms / 900) % 5;
  const char* labels[5] = {
      "WiFi uplink synced",
      "DMA pipeline stable",
      "Touch controller online",
      "Frame budget under 6 ms",
      "UI sprite push complete",
  };

  for (int i = 0; i < 5; ++i) {
    const int16_t y = top + i * line_h;
    const bool focus = (i == active);
    const uint16_t row_bg = focus ? kTheme.panel_alt : kTheme.panel;
    const uint16_t pulse = focus ? kTheme.aqua : kTheme.muted;

    spr.fillRoundRect(8, y, w - 16, 18, 7, row_bg);
    spr.fillCircle(20, y + 9, 4, pulse);
    spr.setTextColor(kTheme.text, row_bg);
    spr.drawString(String(i + 1) + ". " + labels[i], 34, y + 3, 2);
  }
}

}  // namespace

DemoApp::DemoApp(TFT_eSPI& tft)
    : tft_(tft),
      screen_(&tft),
      widget_(&tft),
      touch_spi_(HSPI),
      touch_(kTouchCsPin),
      prefs_(),
      touch_cal_{200, 3600, 200, 3600, 0},
      use_screen_sprite_(false),
      use_sprite_(false),
      touch_ready_(false),
      boot_button_down_(false),
      boot_long_handled_(false),
      touch_button_latched_(false),
      pressed_button_(kNavNone),
      current_scene_(kSceneOverview),
      scene_start_ms_(0),
      last_anim_ms_(0),
      last_input_ms_(0),
      boot_press_start_ms_(0) {}

bool DemoApp::setupTouch() {
  prefs_.begin(kPrefsNamespace, false);

  touch_spi_.begin(kTouchClkPin, kTouchMisoPin, kTouchMosiPin, kTouchCsPin);
  touch_.begin(touch_spi_);
  touch_.setRotation(1);

  if (prefs_.getBytesLength(kTouchCalKey) == sizeof(touch_cal_)) {
    prefs_.getBytes(kTouchCalKey, touch_cal_, sizeof(touch_cal_));
    return true;
  }

  memcpy(touch_cal_, kDefaultTouchCal, sizeof(touch_cal_));
  return true;
}

bool DemoApp::recalibrateTouch() {
  prefs_.remove(kTouchCalKey);
  pressed_button_ = kNavNone;
  touch_button_latched_ = false;

  if (!calibrateTouchInset(tft_, touch_, touch_cal_)) {
    touch_ready_ = false;
    enterScene(current_scene_);
    return false;
  }

  prefs_.putBytes(kTouchCalKey, touch_cal_, sizeof(touch_cal_));
  touch_ready_ = true;
  enterScene(current_scene_);
  return true;
}

bool DemoApp::getTouchPoint(uint16_t* x, uint16_t* y) {
  if (!touch_.touched()) {
    return false;
  }

  const TS_Point p = touch_.getPoint();
  if (p.z < kTouchThreshold) {
    return false;
  }

  int32_t raw_x = p.x;
  int32_t raw_y = p.y;
  int32_t xx = 0;
  int32_t yy = 0;

  if (touch_cal_[4] & 0x01) {
    xx = (raw_y - touch_cal_[0]) * tft_.width() / touch_cal_[1];
    yy = (raw_x - touch_cal_[2]) * tft_.height() / touch_cal_[3];
  } else {
    xx = (raw_x - touch_cal_[0]) * tft_.width() / touch_cal_[1];
    yy = (raw_y - touch_cal_[2]) * tft_.height() / touch_cal_[3];
  }

  if (touch_cal_[4] & 0x02) {
    xx = (tft_.width() - 1) - xx;
  }
  if (touch_cal_[4] & 0x04) {
    yy = (tft_.height() - 1) - yy;
  }

  if (xx < 0 || yy < 0 || xx >= tft_.width() || yy >= tft_.height()) {
    return false;
  }

  *x = static_cast<uint16_t>(xx);
  *y = static_cast<uint16_t>(yy);
  return true;
}

void DemoApp::drawNavBar(TFT_eSPI& canvas, NavButton pressed) {
  canvas.fillRect(0, 204, canvas.width(), 36, kTheme.panel_alt);
  canvas.drawFastHLine(0, 204, canvas.width(), kTheme.blue);

  navButton(canvas, kPrevButtonRect, "PREV", kTheme.aqua, pressed == kNavPrev);
  navButton(canvas, kNextButtonRect, "NEXT", kTheme.orange, pressed == kNavNext);

  canvas.setTextColor(touch_ready_ ? kTheme.text : kTheme.yellow, kTheme.panel_alt);
  canvas.setTextDatum(MC_DATUM);
  const char* center = touch_ready_ ? "Touch | BOOT hold = recal" : "Touch disabled, hold BOOT";
  canvas.drawString(center, canvas.width() / 2, 220, 2);
  canvas.setTextDatum(TL_DATUM);
}

void DemoApp::drawSceneChrome(TFT_eSPI& canvas, Scene scene) {
  header(canvas, kSceneMeta[scene].title, static_cast<uint8_t>(scene));
  canvas.setTextColor(kTheme.muted, kTheme.bg);
  canvas.drawString(kSceneMeta[scene].subtitle, 12, 62, 2);
  drawNavBar(canvas, kNavNone);
}

void DemoApp::renderSceneBase(Scene scene) {
  if (use_screen_sprite_) {
    screen_.fillScreen(kTheme.bg);
    drawSceneChrome(screen_, scene);

    switch (scene) {
      case kSceneOverview:
        card(screen_, 12, 82, 142, 52, kTheme.aqua);
        card(screen_, 166, 82, 142, 52, kTheme.blue);
        card(screen_, 12, 142, 296, 54, kTheme.orange);
        screen_.setTextColor(kTheme.muted, kTheme.panel);
        screen_.drawString("Display", 24, 92, 2);
        screen_.drawString("ESP32", 178, 92, 2);
        screen_.drawString("Runtime", 24, 152, 2);
        screen_.setTextColor(kTheme.text, kTheme.panel);
        screen_.drawString(String(tft_.width()) + "x" + String(tft_.height()), 24, 108, 4);
        screen_.drawString(ESP.getChipModel(), 178, 108, 4);
        break;

      case kSceneControl:
      case kSceneSignals:
      case kSceneMotion:
      case kSceneSensors:
      case kSceneConsole:
        card(screen_, kWidgetArea.x, kWidgetArea.y, kWidgetArea.w, kWidgetArea.h, kTheme.panel_alt);
        break;
    }

    screen_.pushSprite(0, 0);
    return;
  }

  tft_.fillScreen(kTheme.bg);
  drawSceneChrome(tft_, scene);

  switch (scene) {
    case kSceneOverview:
      card(tft_, 12, 82, 142, 52, kTheme.aqua);
      card(tft_, 166, 82, 142, 52, kTheme.blue);
      card(tft_, 12, 142, 296, 54, kTheme.orange);
      tft_.setTextColor(kTheme.muted, kTheme.panel);
      tft_.drawString("Display", 24, 92, 2);
      tft_.drawString("ESP32", 178, 92, 2);
      tft_.drawString("Runtime", 24, 152, 2);
      tft_.setTextColor(kTheme.text, kTheme.panel);
      tft_.drawString(String(tft_.width()) + "x" + String(tft_.height()), 24, 108, 4);
      tft_.drawString(ESP.getChipModel(), 178, 108, 4);
      break;

    case kSceneControl:
    case kSceneSignals:
    case kSceneMotion:
    case kSceneSensors:
    case kSceneConsole:
      card(tft_, kWidgetArea.x, kWidgetArea.y, kWidgetArea.w, kWidgetArea.h, kTheme.panel_alt);
      break;
  }
}

void DemoApp::enterScene(Scene scene) {
  current_scene_ = scene;
  scene_start_ms_ = millis();
  last_anim_ms_ = 0;
  renderSceneBase(scene);
  animateScene(scene, scene_start_ms_, 0);
}

void DemoApp::setPressedButton(NavButton button) {
  if (pressed_button_ == button) {
    return;
  }

  pressed_button_ = button;
  drawNavBar(tft_, pressed_button_);
}

void DemoApp::navigate(int8_t direction) {
  const int next = (static_cast<int>(current_scene_) + direction + kSceneCount) % kSceneCount;
  enterScene(static_cast<Scene>(next));
}

void DemoApp::handleInput(uint32_t now) {
  if ((now - last_input_ms_) < 40) {
    return;
  }
  last_input_ms_ = now;

  const bool boot_pressed = digitalRead(kBootButtonPin) == LOW;
  if (boot_pressed && !boot_button_down_) {
    boot_button_down_ = true;
    boot_long_handled_ = false;
    boot_press_start_ms_ = now;
  } else if (boot_pressed && !boot_long_handled_ && (now - boot_press_start_ms_) >= 1500) {
    boot_long_handled_ = true;
    recalibrateTouch();
    return;
  } else if (!boot_pressed && boot_button_down_) {
    if (!boot_long_handled_) {
      navigate(1);
    }
    boot_button_down_ = false;
    boot_long_handled_ = false;
  }

  if (!touch_ready_) {
    return;
  }

  uint16_t x = 0;
  uint16_t y = 0;
  const bool touching = getTouchPoint(&x, &y);

  if (touching) {
    if (contains(kPrevButtonRect, x, y)) {
      setPressedButton(kNavPrev);
      if (!touch_button_latched_) {
        touch_button_latched_ = true;
        navigate(-1);
      }
    } else if (contains(kNextButtonRect, x, y)) {
      setPressedButton(kNavNext);
      if (!touch_button_latched_) {
        touch_button_latched_ = true;
        navigate(1);
      }
    } else {
      setPressedButton(kNavNone);
      touch_button_latched_ = false;
    }
    return;
  }

  touch_button_latched_ = false;
  setPressedButton(kNavNone);
}

void DemoApp::begin() {
  pinMode(kBootButtonPin, INPUT_PULLUP);

  screen_.setColorDepth(8);
  use_screen_sprite_ = screen_.createSprite(tft_.width(), tft_.height()) != nullptr;

  widget_.setColorDepth(8);
  use_sprite_ = widget_.createSprite(kWidgetArea.w, kWidgetArea.h) != nullptr;

  touch_ready_ = setupTouch();
  enterScene(kSceneOverview);
}

void DemoApp::update() {
  const uint32_t now = millis();
  handleInput(now);

  if ((now - last_anim_ms_) >= kAnimIntervalMs) {
    animateScene(current_scene_, now, now - scene_start_ms_);
    last_anim_ms_ = now;
  }
}

void DemoApp::animateScene(Scene scene, uint32_t elapsed_ms, uint32_t scene_ms) {
  if (!use_sprite_) {
    return;
  }

  switch (scene) {
    case kSceneOverview: {
      drawBarsSprite(widget_, 142, 52,
                     wave01(elapsed_ms, 0.0029f),
                     wave01(elapsed_ms, 0.0021f, 1.1f),
                     wave01(elapsed_ms, 0.0035f, 2.2f));
      widget_.pushSprite(166, 82, 0, 0, 142, 52);

      clearSpriteRegion(widget_, 296, 54, kTheme.panel);
      widget_.drawRoundRect(0, 0, 296, 54, 10, kTheme.orange);
      widget_.setTextColor(kTheme.text, kTheme.panel);
      widget_.drawString(String(scene_ms / 1000) + "s  |  Heap " + String(ESP.getFreeHeap() / 1024) + " KB", 12, 10, 4);
      widget_.drawString("Flash " + String(ESP.getFlashChipSize() / (1024UL * 1024UL)) + " MB  |  CPU " + String(ESP.getCpuFreqMHz()) + " MHz", 12, 34, 2);
      widget_.pushSprite(12, 142, 0, 0, 296, 54);
      break;
    }

    case kSceneControl: {
      drawDialSprite(widget_, 144, 80, wave01(elapsed_ms, 0.0031f), kTheme.aqua, "Activity");
      widget_.pushSprite(12, 82, 0, 0, 144, 80);

      drawDialSprite(widget_, 144, 80, wave01(elapsed_ms, 0.0024f, 1.6f), kTheme.blue, "Network");
      widget_.pushSprite(164, 82, 0, 0, 144, 80);

      drawBarsSprite(widget_, 296, 38,
                     wave01(elapsed_ms, 0.0022f),
                     wave01(elapsed_ms, 0.0028f, 1.4f),
                     wave01(elapsed_ms, 0.0033f, 2.7f));
      widget_.pushSprite(12, 166, 0, 0, 296, 38);
      break;
    }

    case kSceneSignals:
      drawGraphSprite(widget_, 300, 138, scene_ms);
      widget_.pushSprite(kWidgetArea.x, kWidgetArea.y, 0, 0, 300, 138);
      break;

    case kSceneMotion:
      drawMotionSprite(widget_, 300, 138, scene_ms);
      widget_.pushSprite(kWidgetArea.x, kWidgetArea.y, 0, 0, 300, 138);
      break;

    case kSceneSensors:
      drawSensorGridSprite(widget_, 300, 138,
                           wave01(elapsed_ms, 0.0017f, 0.2f),
                           wave01(elapsed_ms, 0.0022f, 1.4f),
                           wave01(elapsed_ms, 0.0012f, 2.1f),
                           wave01(elapsed_ms, 0.0027f, 0.8f));
      widget_.pushSprite(kWidgetArea.x, kWidgetArea.y, 0, 0, 300, 138);
      break;

    case kSceneConsole:
      drawConsoleSprite(widget_, 300, 138, scene_ms);
      widget_.pushSprite(kWidgetArea.x, kWidgetArea.y, 0, 0, 300, 138);
      break;
  }
}
