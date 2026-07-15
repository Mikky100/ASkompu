#pragma once

#include <Arduino.h>

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
  bool consumeRepeatEvent();

 private:
  bool readPressed() const;

  const uint8_t pin_;
  const uint8_t activeLevel_;
  const uint32_t debounceMs_;
  const uint32_t longPressDelayMs_;
  const uint32_t repeatIntervalMs_;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool pressedEventPending_ = false;
  bool repeatEventPending_ = false;
  uint32_t rawChangedAtMs_ = 0;
  uint32_t nextRepeatAtMs_ = 0;
};

}  // namespace input
