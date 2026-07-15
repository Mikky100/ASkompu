#pragma once

#include <Arduino.h>

namespace input {

class DebouncedButton {
 public:
  DebouncedButton(uint8_t pin, uint8_t activeLevel, uint32_t debounceMs);

  void begin();
  void update(uint32_t nowMs);
  bool isPressed() const { return stablePressed_; }
  bool consumePressedEvent();

 private:
  bool readPressed() const;

  const uint8_t pin_;
  const uint8_t activeLevel_;
  const uint32_t debounceMs_;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool pressedEventPending_ = false;
  uint32_t rawChangedAtMs_ = 0;
};

}  // namespace input

