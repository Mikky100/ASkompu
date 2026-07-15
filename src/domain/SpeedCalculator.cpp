#include "SpeedCalculator.h"

namespace domain {

SpeedCalculator::SpeedCalculator(uint32_t millimetersPerPulse,
                                 uint32_t zeroSpeedTimeoutUs)
    : millimetersPerPulse_(millimetersPerPulse),
      zeroSpeedTimeoutUs_(zeroSpeedTimeoutUs) {}

void SpeedCalculator::update(uint32_t nowUs, uint32_t totalPulses,
                             uint32_t previousPulseAtUs,
                             uint32_t lastPulseAtUs) {
  if (totalPulses >= 2 && lastPulseAtUs != calculatedAtPulseUs_) {
    const uint32_t intervalUs = lastPulseAtUs - previousPulseAtUs;
    if (intervalUs > 0) {
      speedKmh_ = static_cast<float>(millimetersPerPulse_) * 3600.0F /
                  static_cast<float>(intervalUs);
    }
    calculatedAtPulseUs_ = lastPulseAtUs;
  }

  if (totalPulses == 0 ||
      nowUs - lastPulseAtUs >= zeroSpeedTimeoutUs_) {
    speedKmh_ = 0.0F;
  }
}

}  // namespace domain
