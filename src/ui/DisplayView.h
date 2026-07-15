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
  void showDriveScreen();
  void showSpeed(float speedKmh);
  void showDiagnostics(uint32_t totalPulses, uint64_t tripDistanceMm);
  void showCalibration(uint32_t millimetersPerPulse, bool saveFailed);
  void showButtonState(ButtonIndicator indicator, bool pressed);

 private:
  void drawButton(ButtonIndicator indicator, bool pressed);

  TFT_eSPI display_;
  uint32_t displayedSpeedTenths_ = UINT32_MAX;
  uint32_t displayedTotalPulses_ = UINT32_MAX;
  uint64_t displayedTripDistanceMm_ = UINT64_MAX;
  uint32_t displayedCalibration_ = UINT32_MAX;
  bool displayedSaveFailed_ = false;
  bool calibrationInitialized_ = false;
  bool displayedButtonStates_[static_cast<uint8_t>(ButtonIndicator::Count)]{};
  bool buttonStateInitialized_[static_cast<uint8_t>(ButtonIndicator::Count)]{};
};

}  // namespace ui
