#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace domain {

struct Palette {
  uint16_t bg;
  uint16_t fg;
  uint16_t accent;
  uint16_t dim;
};

enum class Theme : uint8_t { NightBlue = 0, Amber = 1, Neon = 2 };

inline const Palette& paletteForTheme(Theme theme) {
  static const Palette kNightBlue{TFT_BLACK, TFT_CYAN, TFT_GREEN, TFT_DARKGREY};
  static const Palette kAmber{TFT_BLACK, TFT_ORANGE, TFT_YELLOW, TFT_DARKGREY};
  static const Palette kNeon{TFT_NAVY, TFT_MAGENTA, TFT_GREENYELLOW, TFT_DARKGREY};

  switch (theme) {
    case Theme::Amber:
      return kAmber;
    case Theme::Neon:
      return kNeon;
    default:
      return kNightBlue;
  }
}

}  // namespace domain
