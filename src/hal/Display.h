#pragma once

#include <TFT_eSPI.h>

namespace hal {

class Display {
 public:
  void begin();
  TFT_eSPI& tft() { return tft_; }

 private:
  TFT_eSPI tft_;
};

}  // namespace hal
