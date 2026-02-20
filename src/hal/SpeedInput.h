#pragma once

#include <Arduino.h>

namespace hal {

class SpeedInput {
 public:
  void begin(uint8_t pin, bool risingEdge);
  uint32_t pulseCount() const;
  uint32_t consumeWindowPulses();

 private:
  static void IRAM_ATTR isrThunk();
  void IRAM_ATTR onPulse();

  static SpeedInput* instance_;
  volatile uint32_t pulseCount_ = 0;
  volatile uint32_t windowPulses_ = 0;
};

}  // namespace hal
