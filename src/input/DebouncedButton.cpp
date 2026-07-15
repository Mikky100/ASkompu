#include "DebouncedButton.h"

namespace input {

DebouncedButton::DebouncedButton(uint8_t pin, uint8_t activeLevel,
                                 uint32_t debounceMs,
                                 uint32_t longPressDelayMs,
                                 uint32_t repeatIntervalMs)
    : pin_(pin),
      activeLevel_(activeLevel),
      debounceMs_(debounceMs),
      longPressDelayMs_(longPressDelayMs),
      repeatIntervalMs_(repeatIntervalMs) {}

void DebouncedButton::begin() {
  pinMode(pin_, INPUT_PULLUP);
  rawPressed_ = readPressed();
  stablePressed_ = rawPressed_;
  pressedEventPending_ = false;
  repeatEventPending_ = false;
  rawChangedAtMs_ = millis();
  nextRepeatAtMs_ = rawChangedAtMs_ + longPressDelayMs_;
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
      repeatEventPending_ = false;
      nextRepeatAtMs_ = nowMs + longPressDelayMs_;
    } else {
      repeatEventPending_ = false;
    }
  }

  if (stablePressed_ && longPressDelayMs_ > 0 && repeatIntervalMs_ > 0 &&
      static_cast<int32_t>(nowMs - nextRepeatAtMs_) >= 0) {
    repeatEventPending_ = true;
    nextRepeatAtMs_ += repeatIntervalMs_;
    if (static_cast<int32_t>(nowMs - nextRepeatAtMs_) >= 0) {
      nextRepeatAtMs_ = nowMs + repeatIntervalMs_;
    }
  }
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
