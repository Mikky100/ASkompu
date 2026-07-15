#pragma once

#include <Arduino.h>

#include "ButtonInterpreter.h"

namespace input {

class DebouncedButton {
 public:
  DebouncedButton(uint8_t pin, uint8_t activeLevel, uint32_t debounceMs,
                  uint32_t longPressDelayMs = 0,
                  uint32_t repeatIntervalMs = 0);

  void begin();
  void update(uint32_t nowMs);
  bool isPressed() const { return stablePressed_; }
  bool consumePressedEvent();
  bool consumeReleasedEvent();
  bool consumeLongPressEvent();
  bool consumeRepeatEvent();

 private:
  bool readPressed() const;

  const uint8_t pin_;
  const uint8_t activeLevel_;
  ButtonInterpreter interpreter_;
  bool stablePressed_ = false;
  bool pressedEventPending_ = false;
  bool releasedEventPending_ = false;
  bool longPressEventPending_ = false;
  bool repeatEventPending_ = false;
};

}  // namespace input
