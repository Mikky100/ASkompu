#pragma once

#include <cstdint>

namespace core {

enum class ButtonId : uint8_t {
  Up,
  Down,
  Left,
  Right,
  Point,
  At,
  Trip1Reset,
  Trip2Reset,
  FootReset,
};

enum class ButtonEventType : uint8_t {
  Press,
  Release,
  LongStart,
  LongRepeat,
};

struct ButtonEvent {
  ButtonId buttonId;
  ButtonEventType eventType;
  uint32_t monotonicMs;
};

struct DistancePulseEvent {
  uint32_t pulseCount;
  uint32_t previousPulseAtUs;
  uint32_t lastPulseAtUs;
  uint32_t observedAtUs;
};

}  // namespace core
