#pragma once

#include <cstdint>

namespace settings {

struct CalibrationLoadResult {
  uint32_t millimetersPerPulse;
  bool usedDefault;
};

class SettingsRepository {
 public:
  CalibrationLoadResult loadCalibration() const;
  bool saveCalibration(uint32_t millimetersPerPulse) const;
};

}  // namespace settings
