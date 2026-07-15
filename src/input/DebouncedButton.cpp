#include "DebouncedButton.h"

namespace input {

DebouncedButton::DebouncedButton(uint8_t pin, uint8_t activeLevel,
                                 uint32_t debounceMs)
    : pin_(pin), activeLevel_(activeLevel), debounceMs_(debounceMs) {}

void DebouncedButton::begin() {
  pinMode(pin_, INPUT_PULLUP);
  rawPressed_ = readPressed();
  stablePressed_ = rawPressed_;
  pressedEventPending_ = false;
  rawChangedAtMs_ = millis();
}

void DebouncedButton::update(uint32_t nowMs) {
  const bool pressed = readPressed();

  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    rawChangedAtMs_ = nowMs;
  }

  if (rawPressed_ != stablePressed_ &&
      nowMs - rawChangedAtMs_ >= debounceMs_) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressedEventPending_ = true;
    }
  }
}

bool DebouncedButton::consumePressedEvent() {
  const bool event = pressedEventPending_;
  pressedEventPending_ = false;
  return event;
}

bool DebouncedButton::readPressed() const {
  return digitalRead(pin_) == activeLevel_;
}

}  // namespace input

