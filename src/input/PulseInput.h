#pragma once

#include <Arduino.h>

namespace input {

class PulseInput {
 public:
  PulseInput(uint8_t pin, uint8_t inputMode, int interruptMode);

  void begin();
  uint32_t consumePulses();

 private:
  static void IRAM_ATTR interruptHandler(void* argument);
  void IRAM_ATTR onPulse();

  const uint8_t pin_;
  const uint8_t inputMode_;
  const int interruptMode_;
  volatile uint32_t pendingPulses_ = 0;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace input
