#include "Buttons.h"

#include "BoardConfig.h"

namespace {
constexpr unsigned long kDebounceMs = 35;
constexpr unsigned long kRepeatMs = 250;
}

namespace hal {

void Buttons::begin() {
  states_[0].pin = BoardConfig::PIN_BUTTON_UP;
  states_[0].activeLow = BoardConfig::BUTTONS_ACTIVE_LOW;

  states_[1].pin = BoardConfig::PIN_BUTTON_DOWN;
  states_[1].activeLow = BoardConfig::BUTTONS_ACTIVE_LOW;

  states_[2].pin = BoardConfig::PIN_BUTTON_LEFT;
  states_[2].activeLow = BoardConfig::BUTTONS_ACTIVE_LOW;

  states_[3].pin = BoardConfig::PIN_BUTTON_RIGHT;
  states_[3].activeLow = BoardConfig::BUTTONS_ACTIVE_LOW;

  states_[4].pin = BoardConfig::PIN_TRIP_RESET;
  states_[4].activeLow = BoardConfig::TRIP_RESET_ACTIVE_LOW;

  for (auto& state : states_) {
    state.stablePressed = false;
    state.lastReadPressed = false;
    state.lastChangeMs = 0;
    state.lastRepeatMs = 0;

    pinMode(state.pin, INPUT_PULLUP);
    state.lastReadPressed = readPressed(state);
    state.stablePressed = state.lastReadPressed;
    state.lastChangeMs = millis();
  }
}

bool Buttons::readPressed(const BtnState& state) const {
  bool level = digitalRead(state.pin);
  return state.activeLow ? !level : level;
}

void Buttons::pushEvent(ButtonId id, ButtonEventType type) {
  if (queued_.type == ButtonEventType::None) {
    queued_.id = id;
    queued_.type = type;
  }
}

void Buttons::update() {
  const unsigned long now = millis();
  for (uint8_t i = 0; i < 5; ++i) {
    auto& s = states_[i];
    bool pressed = readPressed(s);

    if (pressed != s.lastReadPressed) {
      s.lastReadPressed = pressed;
      s.lastChangeMs = now;
    }

    if ((now - s.lastChangeMs) >= kDebounceMs && pressed != s.stablePressed) {
      s.stablePressed = pressed;
      ButtonId id = static_cast<ButtonId>(i);
      pushEvent(id, pressed ? ButtonEventType::Pressed : ButtonEventType::Released);
      s.lastRepeatMs = now;
    }

    if (s.stablePressed && (now - s.lastRepeatMs) >= kRepeatMs) {
      s.lastRepeatMs = now;
      pushEvent(static_cast<ButtonId>(i), ButtonEventType::Repeat);
    }
  }
}

ButtonEvent Buttons::popEvent() {
  ButtonEvent ev = queued_;
  queued_.id = ButtonId::None;
  queued_.type = ButtonEventType::None;
  return ev;
}

bool Buttons::isPressed(ButtonId id) const {
  uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= 5) {
    return false;
  }
  return states_[idx].stablePressed;
}

}  // namespace hal
