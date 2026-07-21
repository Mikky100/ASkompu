#pragma once

#include <Arduino.h>

#include "ButtonInterpreter.h"

namespace input {

class DebouncedButton {
 public:
  DebouncedButton(uint8_t pin, uint8_t activeLevel, uint32_t debounceMs,
                  uint32_t longPressDelayMs = 0,
                  uint32_t repeatIntervalMs = 0,
                  uint32_t minimumPressIntervalMs = 150);

  void begin();
  void update(uint32_t nowMs);
  bool isPressed() const { return stablePressed_; }
  bool consumePressedEvent();
  bool consumeReleasedEvent();
  bool consumeShortPressEvent();
  bool consumeLongPressEvent();
  bool consumeRepeatEvent();

 private:
  struct EdgeEvent {
    uint32_t atMs;
    bool pressed;
  };

  static constexpr uint8_t EDGE_QUEUE_SIZE = 16;
  static void ARDUINO_ISR_ATTR handleInterrupt(void* argument);
  bool popEdge(EdgeEvent& event);
  void recordTransitions(const ButtonTransitions& transitions);
  bool readPressed() const;

  const uint8_t pin_;
  const uint8_t activeLevel_;
  ButtonInterpreter interpreter_;
  volatile EdgeEvent edgeQueue_[EDGE_QUEUE_SIZE]{};
  volatile uint8_t edgeWriteIndex_ = 0;
  volatile uint8_t edgeReadIndex_ = 0;
  volatile bool edgeOverflow_ = false;
  bool lastRawPressed_ = false;
  bool stablePressed_ = false;
  bool pressedEventPending_ = false;
  bool releasedEventPending_ = false;
  bool shortPressEventPending_ = false;
  bool longPressEventPending_ = false;
  bool repeatEventPending_ = false;
};

}  // namespace input
