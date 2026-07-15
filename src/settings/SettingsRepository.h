#pragma once

#include <cstdint>

#include "domain/DisplaySetting.h"

namespace settings {

struct CalibrationLoadResult {
  uint32_t millimetersPerPulse;
  bool usedDefault;
};

struct TextColorLoadResult {
  domain::TextColor color;
  bool usedDefault;
};

class SettingsRepository {
 public:
  CalibrationLoadResult loadCalibration() const;
  bool saveCalibration(uint32_t millimetersPerPulse) const;
  TextColorLoadResult loadTextColor() const;
  bool saveTextColor(domain::TextColor color) const;
};

}  // namespace settings
