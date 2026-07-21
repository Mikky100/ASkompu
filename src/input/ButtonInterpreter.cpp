#include "ButtonInterpreter.h"

namespace input {
namespace {

bool hasElapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t intervalMs) {
  return static_cast<int32_t>(nowMs - sinceMs) >=
         static_cast<int32_t>(intervalMs);
}

}  // namespace

ButtonInterpreter::ButtonInterpreter(uint32_t debounceMs,
                                     uint32_t longPressDelayMs,
                                     uint32_t repeatIntervalMs,
                                     uint32_t minimumPressIntervalMs)
    : debounceMs_(debounceMs),
      longPressDelayMs_(longPressDelayMs),
      repeatIntervalMs_(repeatIntervalMs),
      minimumPressIntervalMs_(minimumPressIntervalMs) {}

void ButtonInterpreter::reset(bool pressed, uint32_t nowMs) {
  rawPressed_ = pressed;
  stablePressed_ = pressed;
  longStarted_ = false;
  rawChangedAtMs_ = nowMs;
  pressedAtMs_ = nowMs;
  nextRepeatAtMs_ = nowMs + longPressDelayMs_;
  lastAcceptedPressAtMs_ = nowMs;
  hasAcceptedPress_ = pressed;
  suppressCurrentPress_ = false;
}

ButtonTransitions ButtonInterpreter::update(bool rawPressed, uint32_t nowMs) {
  ButtonTransitions transitions;
  if (rawPressed != rawPressed_) {
    rawPressed_ = rawPressed;
    rawChangedAtMs_ = nowMs;
  }

  if (rawPressed_ != stablePressed_ &&
      hasElapsed(nowMs, rawChangedAtMs_, debounceMs_)) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      suppressCurrentPress_ =
          hasAcceptedPress_ &&
          !hasElapsed(nowMs, lastAcceptedPressAtMs_,
                      minimumPressIntervalMs_);
      transitions.pressed = !suppressCurrentPress_;
      if (!suppressCurrentPress_) {
        lastAcceptedPressAtMs_ = nowMs;
        hasAcceptedPress_ = true;
      }
      longStarted_ = false;
      pressedAtMs_ = nowMs;
      nextRepeatAtMs_ = nowMs + longPressDelayMs_;
    } else {
      transitions.released = true;
      transitions.shortPress = !longStarted_ && !suppressCurrentPress_;
      suppressCurrentPress_ = false;
    }
  }

  if (stablePressed_ && !suppressCurrentPress_ && !longStarted_ &&
      longPressDelayMs_ > 0 &&
      hasElapsed(nowMs, pressedAtMs_, longPressDelayMs_)) {
    longStarted_ = true;
    transitions.longStart = true;
    nextRepeatAtMs_ = nowMs + repeatIntervalMs_;
  } else if (stablePressed_ && longStarted_ && repeatIntervalMs_ > 0 &&
             static_cast<int32_t>(nowMs - nextRepeatAtMs_) >= 0) {
    transitions.longRepeat = true;
    nextRepeatAtMs_ += repeatIntervalMs_;
    if (static_cast<int32_t>(nowMs - nextRepeatAtMs_) >= 0) {
      nextRepeatAtMs_ = nowMs + repeatIntervalMs_;
    }
  }
  return transitions;
}

}  // namespace input
