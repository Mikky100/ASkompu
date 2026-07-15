#include "PulseInput.h"

namespace input {

PulseInput::PulseInput(uint8_t pin, uint8_t inputMode, int interruptMode)
    : pin_(pin), inputMode_(inputMode), interruptMode_(interruptMode) {}

void PulseInput::begin() {
  pinMode(pin_, inputMode_);
  attachInterruptArg(digitalPinToInterrupt(pin_), interruptHandler, this,
                     interruptMode_);
}

uint32_t PulseInput::consumePulses() {
  portENTER_CRITICAL(&mux_);
  const uint32_t pulses = pendingPulses_;
  pendingPulses_ = 0;
  portEXIT_CRITICAL(&mux_);
  return pulses;
}

void IRAM_ATTR PulseInput::interruptHandler(void* argument) {
  static_cast<PulseInput*>(argument)->onPulse();
}

void IRAM_ATTR PulseInput::onPulse() {
  portENTER_CRITICAL_ISR(&mux_);
  if (pendingPulses_ != UINT32_MAX) {
    ++pendingPulses_;
  }
  portEXIT_CRITICAL_ISR(&mux_);
}

}  // namespace input
