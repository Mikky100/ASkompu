#include "DebouncedButton.h"

namespace input {

DebouncedButton::DebouncedButton(uint8_t pin, uint8_t activeLevel,
                                 uint32_t debounceMs,
                                 uint32_t longPressDelayMs,
                                 uint32_t repeatIntervalMs)
    : pin_(pin),
      activeLevel_(activeLevel),
      interpreter_(debounceMs, longPressDelayMs, repeatIntervalMs) {}

void DebouncedButton::begin() {
  pinMode(pin_, INPUT_PULLUP);
  const uint32_t nowMs = millis();
  lastRawPressed_ = readPressed();
  interpreter_.reset(lastRawPressed_, nowMs);
  stablePressed_ = interpreter_.isPressed();
  edgeWriteIndex_ = 0;
  edgeReadIndex_ = 0;
  edgeOverflow_ = false;
  pressedEventPending_ = false;
  releasedEventPending_ = false;
  shortPressEventPending_ = false;
  longPressEventPending_ = false;
  repeatEventPending_ = false;
  attachInterruptArg(pin_, handleInterrupt, this, CHANGE);
}

void DebouncedButton::update(uint32_t nowMs) {
  EdgeEvent edge{};
  while (popEdge(edge)) {
    // First let the previous raw level settle up to this edge timestamp. This
    // preserves a complete press/release that occurred during a blocking draw.
    recordTransitions(interpreter_.update(lastRawPressed_, edge.atMs));
    lastRawPressed_ = edge.pressed;
    recordTransitions(interpreter_.update(lastRawPressed_, edge.atMs));
  }

  if (edgeOverflow_) {
    noInterrupts();
    edgeOverflow_ = false;
    interrupts();
    lastRawPressed_ = readPressed();
  }
  recordTransitions(interpreter_.update(lastRawPressed_, nowMs));
  stablePressed_ = interpreter_.isPressed();
}

void ARDUINO_ISR_ATTR DebouncedButton::handleInterrupt(void* argument) {
  auto* button = static_cast<DebouncedButton*>(argument);
  const uint8_t writeIndex = button->edgeWriteIndex_;
  const uint8_t nextIndex =
      static_cast<uint8_t>((writeIndex + 1U) % EDGE_QUEUE_SIZE);
  if (nextIndex == button->edgeReadIndex_) {
    button->edgeOverflow_ = true;
    return;
  }
  button->edgeQueue_[writeIndex].atMs =
      static_cast<uint32_t>(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
  button->edgeQueue_[writeIndex].pressed =
      digitalRead(button->pin_) == button->activeLevel_;
  button->edgeWriteIndex_ = nextIndex;
}

bool DebouncedButton::popEdge(EdgeEvent& event) {
  noInterrupts();
  const uint8_t readIndex = edgeReadIndex_;
  if (readIndex == edgeWriteIndex_) {
    interrupts();
    return false;
  }
  event.atMs = edgeQueue_[readIndex].atMs;
  event.pressed = edgeQueue_[readIndex].pressed;
  edgeReadIndex_ = static_cast<uint8_t>((readIndex + 1U) % EDGE_QUEUE_SIZE);
  interrupts();
  return true;
}

void DebouncedButton::recordTransitions(const ButtonTransitions& transitions) {
  pressedEventPending_ = pressedEventPending_ || transitions.pressed;
  releasedEventPending_ = releasedEventPending_ || transitions.released;
  shortPressEventPending_ = shortPressEventPending_ || transitions.shortPress;
  longPressEventPending_ = longPressEventPending_ || transitions.longStart;
  repeatEventPending_ = repeatEventPending_ || transitions.longRepeat;
}

bool DebouncedButton::consumeShortPressEvent() {
  const bool event = shortPressEventPending_;
  shortPressEventPending_ = false;
  return event;
}

bool DebouncedButton::consumeReleasedEvent() {
  const bool event = releasedEventPending_;
  releasedEventPending_ = false;
  return event;
}

bool DebouncedButton::consumeLongPressEvent() {
  const bool event = longPressEventPending_;
  longPressEventPending_ = false;
  return event;
}

bool DebouncedButton::consumePressedEvent() {
  const bool event = pressedEventPending_;
  pressedEventPending_ = false;
  return event;
}

bool DebouncedButton::consumeRepeatEvent() {
  const bool event = repeatEventPending_;
  repeatEventPending_ = false;
  return event;
}

bool DebouncedButton::readPressed() const {
  return digitalRead(pin_) == activeLevel_;
}

}  // namespace input
