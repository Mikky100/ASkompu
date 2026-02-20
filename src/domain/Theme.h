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

enum class Theme : uint8_t { Red = 0, Green = 1, Blue = 2 };

inline const Palette& paletteForTheme(Theme theme) {
  static const Palette kRed{TFT_BLACK, TFT_RED, TFT_MAROON, TFT_DARKGREY};
  static const Palette kGreen{TFT_BLACK, TFT_GREEN, TFT_DARKGREEN, TFT_DARKGREY};
  static const Palette kBlue{TFT_BLACK, TFT_CYAN, TFT_BLUE, TFT_DARKGREY};

  switch (theme) {
    case Theme::Green:
      return kGreen;
    case Theme::Blue:
      return kBlue;
    default:
      return kRed;
  }
}

}  // namespace domain
