#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace ui {

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct Layout {
  TFT_eSPI& tft;
  TFT_eSprite canvas;

  Rect rectClock;
  Rect rectPoints;
  Rect rectTrip;
  Rect rectStatus;
  Rect rectMenu;

  uint8_t fontHuge = 8;
  uint8_t fontLarge = 6;
  uint8_t fontSmall = 2;

  explicit Layout(TFT_eSPI& display);
  void recalc();
};

}  // namespace ui
