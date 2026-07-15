#include "CalibrationSetting.h"

#include "CalibrationConfig.h"

namespace domain {
namespace calibration {

bool isValid(uint32_t millimetersPerPulse) {
  return millimetersPerPulse >= CalibrationConfig::MIN_MILLIMETERS_PER_PULSE &&
         millimetersPerPulse <= CalibrationConfig::MAX_MILLIMETERS_PER_PULSE;
}

uint32_t validatedOrDefault(uint32_t millimetersPerPulse) {
  return isValid(millimetersPerPulse)
             ? millimetersPerPulse
             : CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE;
}

uint32_t increment(uint32_t millimetersPerPulse) {
  if (millimetersPerPulse >= CalibrationConfig::MAX_MILLIMETERS_PER_PULSE -
                                  CalibrationConfig::MILLIMETERS_PER_PULSE_STEP) {
    return CalibrationConfig::MAX_MILLIMETERS_PER_PULSE;
  }
  return millimetersPerPulse +
         CalibrationConfig::MILLIMETERS_PER_PULSE_STEP;
}

uint32_t decrement(uint32_t millimetersPerPulse) {
  if (millimetersPerPulse <= CalibrationConfig::MIN_MILLIMETERS_PER_PULSE +
                                  CalibrationConfig::MILLIMETERS_PER_PULSE_STEP) {
    return CalibrationConfig::MIN_MILLIMETERS_PER_PULSE;
  }
  return millimetersPerPulse -
         CalibrationConfig::MILLIMETERS_PER_PULSE_STEP;
}

}  // namespace calibration
}  // namespace domain
