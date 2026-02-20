#include "SpeedInput.h"

namespace hal {

SpeedInput* SpeedInput::instance_ = nullptr;

void IRAM_ATTR SpeedInput::isrThunk() {
  if (instance_ != nullptr) {
    instance_->onPulse();
  }
}

void IRAM_ATTR SpeedInput::onPulse() {
  pulseCount_++;
  windowPulses_++;
}

void SpeedInput::begin(uint8_t pin, bool risingEdge) {
  instance_ = this;
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), isrThunk,
                  risingEdge ? RISING : FALLING);
}

uint32_t SpeedInput::pulseCount() const {
  noInterrupts();
  uint32_t value = pulseCount_;
  interrupts();
  return value;
}

uint32_t SpeedInput::consumeWindowPulses() {
  noInterrupts();
  uint32_t value = windowPulses_;
  windowPulses_ = 0;
  interrupts();
  return value;
}

}  // namespace hal
