#pragma once

#include <Arduino.h>

#include <cstdint>

namespace ports {

class ArduinoClock {
 public:
  uint32_t monotonicMilliseconds() const { return millis(); }
  uint32_t monotonicMicroseconds() const { return micros(); }
};

}  // namespace ports
