#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

class DemoApp {
 public:
  explicit DemoApp(TFT_eSPI& tft);

  void begin();
  void update();

 private:
  enum Scene : uint8_t {
    kSceneOverview = 0,
    kSceneControl = 1,
    kSceneSignals = 2,
    kSceneMotion = 3,
    kSceneSensors = 4,
    kSceneConsole = 5,
  };

  enum NavButton : uint8_t {
    kNavNone = 0,
    kNavPrev = 1,
    kNavNext = 2,
  };

  TFT_eSPI& tft_;
  TFT_eSprite screen_;
  TFT_eSprite widget_;
  SPIClass touch_spi_;
  XPT2046_Touchscreen touch_;
  Preferences prefs_;
  uint16_t touch_cal_[5];
  bool use_screen_sprite_;
  bool use_sprite_;
  bool touch_ready_;
  bool boot_button_down_;
  bool boot_long_handled_;
  bool touch_button_latched_;
  NavButton pressed_button_;
  Scene current_scene_;
  uint32_t scene_start_ms_;
  uint32_t last_anim_ms_;
  uint32_t last_input_ms_;
  uint32_t boot_press_start_ms_;

  bool setupTouch();
  bool recalibrateTouch();
  bool getTouchPoint(uint16_t* x, uint16_t* y);
  void drawSceneChrome(TFT_eSPI& canvas, Scene scene);
  void drawNavBar(TFT_eSPI& canvas, NavButton pressed);
  void renderSceneBase(Scene scene);
  void enterScene(Scene scene);
  void animateScene(Scene scene, uint32_t elapsed_ms, uint32_t scene_ms);
  void handleInput(uint32_t now);
  void setPressedButton(NavButton button);
  void navigate(int8_t direction);
};
