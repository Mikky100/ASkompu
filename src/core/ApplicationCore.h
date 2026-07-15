#pragma once

#include <cstdint>

#include "AppEvent.h"
#include "DisplayModel.h"
#include "domain/SpeedCalculator.h"

namespace core {

class ApplicationCore {
 public:
  ApplicationCore(uint32_t millimetersPerPulse, uint32_t zeroSpeedTimeoutUs);

  void setInitialMillimetersPerPulse(uint32_t millimetersPerPulse);
  void handleButton(const ButtonEvent& event);
  void handleDistancePulses(const DistancePulseEvent& event);
  void tick(uint32_t nowUs);

  DisplayModel displayModel() const;

  bool takeCalibrationSaveRequest(uint32_t& millimetersPerPulse);
  void completeCalibrationSave(bool succeeded);

  uint64_t trip1DistanceMillimeters() const { return trip1DistanceMm_; }
  uint64_t trip2DistanceMillimeters() const { return trip2DistanceMm_; }
  uint64_t totalPulseCount() const { return totalPulseCount_; }
  uint32_t millimetersPerPulse() const { return millimetersPerPulse_; }
  Screen screen() const { return screen_; }

 private:
  void openCalibration(bool incrementValue);
  void addDistance(uint64_t incrementMillimeters);

  domain::SpeedCalculator speedCalculator_;
  uint32_t millimetersPerPulse_;
  uint32_t editedMillimetersPerPulse_;
  uint64_t trip1DistanceMm_ = 0;
  uint64_t trip2DistanceMm_ = 0;
  uint64_t totalPulseCount_ = 0;
  bool trip1ResetHeld_ = false;
  bool calibrationSaveFailed_ = false;
  bool calibrationSavePending_ = false;
  bool calibrationSaveInFlight_ = false;
  Screen screen_ = Screen::Drive;
  uint32_t previousPulseAtUs_ = 0;
  uint32_t lastPulseAtUs_ = 0;
};

}  // namespace core
