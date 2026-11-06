#pragma once

#include <Arduino.h>
#include "PulseDiagnostics.h"

namespace input {

struct PulseSnapshot {
  uint32_t pendingPulses;
  uint32_t totalPulses;
  uint32_t previousPulseAtUs;
  uint32_t lastPulseAtUs;
};

class PulseInput {
 public:
  PulseInput(uint8_t pin, uint8_t inputMode, int interruptMode);

  void begin();
  void setMinimumPulseIntervalUs(uint32_t minimumIntervalUs);
  PulseSnapshot consumeSnapshot(bool resetDiagnostics = false);
  void recordDelivered(uint32_t count);
  PulseDiagnosticsSnapshot diagnostics();
  void freezeDiagnostics();
  PulseTrace trace(uint8_t index);

 private:
  static void IRAM_ATTR interruptHandler(void* argument);
  void IRAM_ATTR onPulse();

  PulseDiagnostics diagnostics_;
  const uint8_t pin_;
  const uint8_t inputMode_;
  const int interruptMode_;
  volatile uint32_t pendingPulses_ = 0;
  volatile uint32_t totalPulses_ = 0;
  volatile uint32_t previousPulseAtUs_ = 0;
  volatile uint32_t lastPulseAtUs_ = 0;
  volatile uint32_t minimumPulseIntervalUs_ = 0;
  volatile bool hasAcceptedPulse_ = false;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace input
