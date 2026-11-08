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

PulseSnapshot PulseInput::consumeSnapshot(bool resetDiagnostics) {
  portENTER_CRITICAL(&mux_);
  const PulseSnapshot snapshot = {
      pendingPulses_, totalPulses_, previousPulseAtUs_, lastPulseAtUs_};
  pendingPulses_ = 0;
  if (resetDiagnostics) diagnostics_.reset();
  portEXIT_CRITICAL(&mux_);
  return snapshot;
}

void PulseInput::recordDelivered(uint32_t count) {
  portENTER_CRITICAL(&mux_);
  diagnostics_.delivered(count);
  portEXIT_CRITICAL(&mux_);
}

PulseDiagnosticsSnapshot PulseInput::diagnostics() {
  portENTER_CRITICAL(&mux_);
  const PulseDiagnosticsSnapshot result =
      diagnostics_.snapshot(minimumPulseIntervalUs_);
  portEXIT_CRITICAL(&mux_);
  return result;
}

void PulseInput::freezeDiagnostics() {
  portENTER_CRITICAL(&mux_);
  diagnostics_.freeze();
  portEXIT_CRITICAL(&mux_);
}

PulseTrace PulseInput::trace(uint8_t index) {
  portENTER_CRITICAL(&mux_);
  const PulseTrace result = diagnostics_.trace(index);
  portEXIT_CRITICAL(&mux_);
  return result;
}

void IRAM_ATTR PulseInput::interruptHandler(void* argument) {
  static_cast<PulseInput*>(argument)->onPulse();
}

void IRAM_ATTR PulseInput::onPulse() {
  const uint32_t timestampUs = micros();

  portENTER_CRITICAL_ISR(&mux_);
  const bool accepted = acceptsPulseInterval(timestampUs, lastPulseAtUs_,
                                              minimumPulseIntervalUs_,
                                              hasAcceptedPulse_);
  diagnostics_.observe(timestampUs, accepted, hasAcceptedPulse_,
                       lastPulseAtUs_);
  if (!accepted) {
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }
  if (pendingPulses_ != UINT32_MAX) {
    ++pendingPulses_;
  } else {
    diagnostics_.pendingOverflow();
  }
  if (totalPulses_ != UINT32_MAX) {
    ++totalPulses_;
  } else {
    diagnostics_.lifetimeOverflow();
  }
  previousPulseAtUs_ = lastPulseAtUs_;
  lastPulseAtUs_ = timestampUs;
  hasAcceptedPulse_ = true;
  portEXIT_CRITICAL_ISR(&mux_);
}

}  // namespace input
