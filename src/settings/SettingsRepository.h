#pragma once

#include <cstdint>

#include "domain/DisplaySetting.h"
#include "domain/CompetitionRecords.h"

namespace settings {

struct CalibrationLoadResult {
  uint32_t millimetersPerPulse;
  bool usedDefault;
};

struct TextColorLoadResult {
  domain::TextColor color;
  bool usedDefault;
};

struct CompetitionSettingsLoadResult {
  domain::CompetitionSettings settings;
  bool usedDefault;
};

struct DebugDisplaySettingsLoadResult {
  domain::DebugDisplaySettings settings;
  bool usedDefault;
};

class SettingsRepository {
 public:
  CalibrationLoadResult loadCalibration() const;
  bool saveCalibration(uint32_t millimetersPerPulse) const;
  TextColorLoadResult loadTextColor() const;
  bool saveTextColor(domain::TextColor color) const;
  CompetitionSettingsLoadResult loadCompetitionSettings() const;
  bool saveCompetitionSettings(
      const domain::CompetitionSettings& settings) const;
  DebugDisplaySettingsLoadResult loadDebugDisplaySettings() const;
  bool saveDebugDisplaySettings(
      const domain::DebugDisplaySettings& settings) const;
};

}  // namespace settings
