#include "SpeedCalculator.h"

#include "MotionMath.h"

namespace domain {

SpeedCalculator::SpeedCalculator(uint32_t millimetersPerPulse,
                                 uint32_t zeroSpeedTimeoutUs)
    : millimetersPerPulse_(millimetersPerPulse),
      zeroSpeedTimeoutUs_(zeroSpeedTimeoutUs) {}

void SpeedCalculator::setMillimetersPerPulse(uint32_t millimetersPerPulse) {
  millimetersPerPulse_ = millimetersPerPulse;
}

void SpeedCalculator::update(uint32_t nowUs, uint32_t totalPulses,
                             uint32_t previousPulseAtUs,
                             uint32_t lastPulseAtUs) {
  if (totalPulses >= 2 && lastPulseAtUs != calculatedAtPulseUs_) {
    const uint32_t intervalUs = lastPulseAtUs - previousPulseAtUs;
    if (intervalUs > 0) {
      speedKmh_ = motion::speedKmhFromPulseInterval(millimetersPerPulse_,
                                                     intervalUs);
    }
    calculatedAtPulseUs_ = lastPulseAtUs;
  }

  if (totalPulses == 0 ||
      motion::elapsedAtLeast(nowUs, lastPulseAtUs, zeroSpeedTimeoutUs_)) {
    speedKmh_ = 0.0F;
  }
}

}  // namespace domain
