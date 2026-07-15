#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "core/DisplayModel.h"

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
  void render(const core::DisplayModel& model);
  void showButtonState(ButtonIndicator indicator, bool pressed);

 private:
  void showDriveScreen();
  void showSpeed(float speedKmh);
  void showDiagnostics(uint64_t totalPulses, uint64_t trip1DistanceMm,
                       uint64_t trip2DistanceMm);
  void showCalibration(uint32_t millimetersPerPulse, bool saveFailed);
  void drawButton(ButtonIndicator indicator, bool pressed);

  TFT_eSPI display_;
  uint32_t displayedSpeedTenths_ = UINT32_MAX;
  uint64_t displayedTotalPulses_ = UINT64_MAX;
  uint64_t displayedTrip1DistanceMm_ = UINT64_MAX;
  uint64_t displayedTrip2DistanceMm_ = UINT64_MAX;
  uint32_t displayedCalibration_ = UINT32_MAX;
  bool displayedSaveFailed_ = false;
  bool calibrationInitialized_ = false;
  bool displayedButtonStates_[static_cast<uint8_t>(ButtonIndicator::Count)]{};
  bool buttonStateInitialized_[static_cast<uint8_t>(ButtonIndicator::Count)]{};
  core::Screen displayedScreen_ = core::Screen::Calibration;
};

}  // namespace ui
