#pragma once

#include <cstdint>

namespace input {

struct ButtonTransitions {
  bool pressed = false;
  bool released = false;
  bool shortPress = false;
  bool longStart = false;
  bool longRepeat = false;
};

class ButtonInterpreter {
 public:
  ButtonInterpreter(uint32_t debounceMs, uint32_t longPressDelayMs = 0,
                    uint32_t repeatIntervalMs = 0);

  void reset(bool pressed, uint32_t nowMs);
  ButtonTransitions update(bool rawPressed, uint32_t nowMs);
  bool isPressed() const { return stablePressed_; }

 private:
  const uint32_t debounceMs_;
  const uint32_t longPressDelayMs_;
  const uint32_t repeatIntervalMs_;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool longStarted_ = false;
  uint32_t rawChangedAtMs_ = 0;
  uint32_t pressedAtMs_ = 0;
  uint32_t nextRepeatAtMs_ = 0;
};

}  // namespace input
