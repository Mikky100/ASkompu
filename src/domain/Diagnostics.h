#pragma once

#include <Arduino.h>

namespace domain {

struct Diagnostics {
  float kmh = 0.0f;
  uint32_t pulseCount = 0;
  uint32_t pulsesWindow = 0;
  uint16_t loopHz = 0;
};

}  // namespace domain
