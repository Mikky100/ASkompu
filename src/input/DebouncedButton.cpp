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
  interpreter_.reset(readPressed(), nowMs);
  stablePressed_ = interpreter_.isPressed();
  pressedEventPending_ = false;
  releasedEventPending_ = false;
  shortPressEventPending_ = false;
  longPressEventPending_ = false;
  repeatEventPending_ = false;
}

void DebouncedButton::update(uint32_t nowMs) {
  const ButtonTransitions transitions =
      interpreter_.update(readPressed(), nowMs);
  stablePressed_ = interpreter_.isPressed();
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
