#include "SettingsRepository.h"

#include <Preferences.h>

#include "CalibrationConfig.h"
#include "domain/CalibrationSetting.h"

namespace settings {
namespace {

constexpr char PREFERENCES_NAMESPACE[] = "askompu";
constexpr char SCHEMA_VERSION_KEY[] = "schemaVersion";
constexpr char CALIBRATION_KEY[] = "mmPerPulseFixed";

}  // namespace

CalibrationLoadResult SettingsRepository::loadCalibration() const {
  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, true)) {
    return {CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE, true};
  }

  const uint16_t schemaVersion = preferences.getUShort(SCHEMA_VERSION_KEY, 0);
  const uint32_t storedValue = preferences.getUInt(CALIBRATION_KEY, 0);
  preferences.end();

  const bool valid =
      schemaVersion == CalibrationConfig::SETTINGS_SCHEMA_VERSION &&
      domain::calibration::isValid(storedValue);
  return {valid ? storedValue
                : CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE,
          !valid};
}

bool SettingsRepository::saveCalibration(uint32_t millimetersPerPulse) const {
  if (!domain::calibration::isValid(millimetersPerPulse)) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
    return false;
  }

  const bool alreadyStored =
      preferences.getUShort(SCHEMA_VERSION_KEY, 0) ==
          CalibrationConfig::SETTINGS_SCHEMA_VERSION &&
      preferences.getUInt(CALIBRATION_KEY, 0) == millimetersPerPulse;
  if (alreadyStored) {
    preferences.end();
    return true;
  }

  const bool schemaWritten =
      preferences.putUShort(SCHEMA_VERSION_KEY,
                            CalibrationConfig::SETTINGS_SCHEMA_VERSION) ==
      sizeof(uint16_t);
  const bool calibrationWritten =
      preferences.putUInt(CALIBRATION_KEY, millimetersPerPulse) ==
      sizeof(uint32_t);
  preferences.end();
  return schemaWritten && calibrationWritten;
}

}  // namespace settings
