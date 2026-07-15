#pragma once

#include <Arduino.h>

#include <cstdint>

#include "core/Clock.h"

namespace ports {

class ArduinoClock : public core::TimeSource {
 public:
  uint32_t monotonicMilliseconds() const override { return millis(); }
  uint32_t monotonicMicroseconds() const { return micros(); }
};

}  // namespace ports
