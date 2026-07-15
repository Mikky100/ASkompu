#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace ui {

enum class ButtonIndicator : uint8_t {
  Left = 0,
  Up,
  Down,
  Right,
  TripReset,
  Count,
};

class DisplayView {
 public:
  void begin();
  void showTrip(uint32_t value);
  void showButtonState(ButtonIndicator indicator, bool pressed);

 private:
  void drawButton(ButtonIndicator indicator, bool pressed);

  TFT_eSPI display_;
  uint32_t displayedTrip_ = UINT32_MAX;
  bool displayedButtonStates_[static_cast<uint8_t>(ButtonIndicator::Count)]{};
  bool buttonStateInitialized_[static_cast<uint8_t>(ButtonIndicator::Count)]{};
};

}  // namespace ui

