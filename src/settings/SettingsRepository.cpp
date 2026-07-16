#include "SettingsRepository.h"

#include <Preferences.h>

#include "CalibrationConfig.h"
#include "domain/CalibrationSetting.h"

namespace settings {
namespace {

constexpr char PREFERENCES_NAMESPACE[] = "askompu";
constexpr char SCHEMA_VERSION_KEY[] = "schemaVersion";
constexpr char CALIBRATION_KEY[] = "mmPerPulseFixed";
constexpr char TEXT_COLOR_KEY[] = "textColor";
constexpr char LATE_FACTOR_KEY[] = "lateFactor";
constexpr char EARLY_FACTOR_KEY[] = "earlyFactor";
constexpr char JAT_RESULT_SECONDS_KEY[] = "jatResultSec";
constexpr char AT_DISPLAY_DISTANCE_KEY[] = "atDistanceM";

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
      (schemaVersion == 1 || schemaVersion == 2 ||
       schemaVersion == CalibrationConfig::SETTINGS_SCHEMA_VERSION) &&
      domain::calibration::isValid(storedValue);
  return {valid ? storedValue
                : CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE,
          !valid};
}

TextColorLoadResult SettingsRepository::loadTextColor() const {
  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, true)) {
    return {domain::TextColor::WHITE, true};
  }
  const uint16_t schemaVersion = preferences.getUShort(SCHEMA_VERSION_KEY, 0);
  const domain::TextColor color = static_cast<domain::TextColor>(
      preferences.getUChar(TEXT_COLOR_KEY,
                           static_cast<uint8_t>(domain::TextColor::WHITE)));
  preferences.end();
  const bool valid =
      (schemaVersion == 2 ||
       schemaVersion == CalibrationConfig::SETTINGS_SCHEMA_VERSION) &&
      domain::isValidTextColor(color);
  return {valid ? color : domain::TextColor::WHITE, !valid};
}

bool SettingsRepository::saveTextColor(domain::TextColor color) const {
  if (!domain::isValidTextColor(color)) return false;
  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, false)) return false;
  const bool schemaWritten =
      preferences.putUShort(SCHEMA_VERSION_KEY,
                            CalibrationConfig::SETTINGS_SCHEMA_VERSION) ==
      sizeof(uint16_t);
  const bool colorWritten =
      preferences.putUChar(TEXT_COLOR_KEY, static_cast<uint8_t>(color)) ==
      sizeof(uint8_t);
  preferences.end();
  return schemaWritten && colorWritten;
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

CompetitionSettingsLoadResult SettingsRepository::loadCompetitionSettings()
    const {
  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, true))
    return {domain::CompetitionSettings{}, true};
  const uint16_t schemaVersion = preferences.getUShort(SCHEMA_VERSION_KEY, 0);
  domain::CompetitionSettings loaded;
  bool usedDefault = schemaVersion != CalibrationConfig::SETTINGS_SCHEMA_VERSION;
  if (!usedDefault) {
    loaded.lateFactor = preferences.getUShort(LATE_FACTOR_KEY, 1);
    loaded.earlyFactor = preferences.getUShort(EARLY_FACTOR_KEY, 3);
    loaded.jatResultSeconds = preferences.getUChar(JAT_RESULT_SECONDS_KEY, 5);
    loaded.atDisplayDistanceM =
        preferences.getUChar(AT_DISPLAY_DISTANCE_KEY, 10);
  }
  preferences.end();
  const domain::CompetitionSettings validated =
      domain::validatedCompetitionSettings(loaded);
  usedDefault = usedDefault || validated.lateFactor != loaded.lateFactor ||
                validated.earlyFactor != loaded.earlyFactor ||
                validated.jatResultSeconds != loaded.jatResultSeconds ||
                validated.atDisplayDistanceM != loaded.atDisplayDistanceM;
  return {validated, usedDefault};
}

bool SettingsRepository::saveCompetitionSettings(
    const domain::CompetitionSettings& settings) const {
  const domain::CompetitionSettings validated =
      domain::validatedCompetitionSettings(settings);
  if (validated.lateFactor != settings.lateFactor ||
      validated.earlyFactor != settings.earlyFactor ||
      validated.jatResultSeconds != settings.jatResultSeconds ||
      validated.atDisplayDistanceM != settings.atDisplayDistanceM)
    return false;
  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, false)) return false;
  bool ok = true;
  if (preferences.getUShort(SCHEMA_VERSION_KEY, 0) !=
      CalibrationConfig::SETTINGS_SCHEMA_VERSION)
    ok = preferences.putUShort(SCHEMA_VERSION_KEY,
                               CalibrationConfig::SETTINGS_SCHEMA_VERSION) ==
         sizeof(uint16_t);
  if (ok && preferences.getUShort(LATE_FACTOR_KEY, 0) != validated.lateFactor)
    ok = preferences.putUShort(LATE_FACTOR_KEY, validated.lateFactor) ==
         sizeof(uint16_t);
  if (ok && preferences.getUShort(EARLY_FACTOR_KEY, 0) != validated.earlyFactor)
    ok = preferences.putUShort(EARLY_FACTOR_KEY, validated.earlyFactor) ==
         sizeof(uint16_t);
  if (ok && preferences.getUChar(JAT_RESULT_SECONDS_KEY, 0xFF) !=
                validated.jatResultSeconds)
    ok = preferences.putUChar(JAT_RESULT_SECONDS_KEY,
                              validated.jatResultSeconds) == sizeof(uint8_t);
  if (ok && preferences.getUChar(AT_DISPLAY_DISTANCE_KEY, 0xFF) !=
                validated.atDisplayDistanceM)
    ok = preferences.putUChar(AT_DISPLAY_DISTANCE_KEY,
                              validated.atDisplayDistanceM) == sizeof(uint8_t);
  preferences.end();
  return ok;
}

}  // namespace settings
