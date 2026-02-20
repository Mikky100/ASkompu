#include "Display.h"

namespace hal {

void Display::begin() {
  tft_.init();
  tft_.setRotation(1);
  tft_.fillScreen(TFT_BLACK);
}

}  // namespace hal
