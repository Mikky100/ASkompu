#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace ui {

struct Layout {
  TFT_eSPI& tft;
  TFT_eSprite canvas;

  TFT_eRect rectClock;
  TFT_eRect rectPoints;
  TFT_eRect rectTrip;
  TFT_eRect rectStatus;
  TFT_eRect rectMenu;

  uint8_t fontHuge = 8;
  uint8_t fontLarge = 6;
  uint8_t fontSmall = 2;

  explicit Layout(TFT_eSPI& display);
  void recalc();
};

}  // namespace ui
