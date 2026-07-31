#include "PulseInput.h"

#include "PulseFilter.h"

namespace input {

PulseInput::PulseInput(uint8_t pin, uint8_t inputMode, int interruptMode)
    : pin_(pin), inputMode_(inputMode), interruptMode_(interruptMode) {}

void PulseInput::begin() {
  pinMode(pin_, inputMode_);
  attachInterruptArg(digitalPinToInterrupt(pin_), interruptHandler, this,
                     interruptMode_);
}

void PulseInput::setMinimumPulseIntervalUs(uint32_t minimumIntervalUs) {
  portENTER_CRITICAL(&mux_);
  minimumPulseIntervalUs_ = minimumIntervalUs;
  portEXIT_CRITICAL(&mux_);
}

PulseSnapshot PulseInput::consumeSnapshot() {
  portENTER_CRITICAL(&mux_);
  const PulseSnapshot snapshot = {
      pendingPulses_, totalPulses_, previousPulseAtUs_, lastPulseAtUs_};
  pendingPulses_ = 0;
  portEXIT_CRITICAL(&mux_);
  return snapshot;
}

void IRAM_ATTR PulseInput::interruptHandler(void* argument) {
  static_cast<PulseInput*>(argument)->onPulse();
}

void IRAM_ATTR PulseInput::onPulse() {
  const uint32_t timestampUs = micros();

  portENTER_CRITICAL_ISR(&mux_);
  if (!acceptsPulseInterval(timestampUs, lastPulseAtUs_,
                            minimumPulseIntervalUs_, hasAcceptedPulse_)) {
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }
  if (pendingPulses_ != UINT32_MAX) {
    ++pendingPulses_;
  }
  if (totalPulses_ != UINT32_MAX) {
    ++totalPulses_;
  }
  previousPulseAtUs_ = lastPulseAtUs_;
  lastPulseAtUs_ = timestampUs;
  hasAcceptedPulse_ = true;
  portEXIT_CRITICAL_ISR(&mux_);
}

}  // namespace input
