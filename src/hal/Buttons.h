#pragma once

#include <Arduino.h>

namespace hal {

enum class ButtonId : uint8_t { Up, Down, Left, Right, TripReset, None };
enum class ButtonEventType : uint8_t { Pressed, Released, Repeat, None };

struct ButtonEvent {
  ButtonId id = ButtonId::None;
  ButtonEventType type = ButtonEventType::None;
};

class Buttons {
 public:
  void begin();
  void update();
  ButtonEvent popEvent();
  bool isPressed(ButtonId id) const;

 private:
  struct BtnState {
    uint8_t pin;
    bool activeLow;
    bool stablePressed = false;
    bool lastReadPressed = false;
    unsigned long lastChangeMs = 0;
    unsigned long lastRepeatMs = 0;
  };

  BtnState states_[5]{};
  ButtonEvent queued_{};

  bool readPressed(const BtnState& state) const;
  void pushEvent(ButtonId id, ButtonEventType type);
};

}  // namespace hal
