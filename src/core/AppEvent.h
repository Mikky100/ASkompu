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
  DistancePulseEvent(uint32_t pulseCountValue, uint32_t previousPulseAtUsValue,
                     uint32_t lastPulseAtUsValue, uint32_t observedAtUsValue,
                     bool reverseActiveValue = false)
      : pulseCount(pulseCountValue),
        previousPulseAtUs(previousPulseAtUsValue),
        lastPulseAtUs(lastPulseAtUsValue),
        observedAtUs(observedAtUsValue),
        reverseActive(reverseActiveValue) {}
  uint32_t pulseCount;
  uint32_t previousPulseAtUs;
  uint32_t lastPulseAtUs;
  uint32_t observedAtUs;
  bool reverseActive;
};

struct ReverseSignalEvent {
  bool reverseActive;
  uint32_t monotonicMs;
};

}  // namespace core
