#include "TripCounter.h"

#include <limits>

#include "MotionMath.h"

namespace domain {

TripCounter::TripCounter(uint32_t millimetersPerPulse)
    : millimetersPerPulse_(millimetersPerPulse) {}

void TripCounter::setMillimetersPerPulse(uint32_t millimetersPerPulse) {
  millimetersPerPulse_ = millimetersPerPulse;
}

void TripCounter::addPulses(uint32_t pulses, bool reverseActive) {
  const uint64_t pulseIncrement = pulses;
  const uint64_t distanceIncrement =
      motion::distanceMillimetersForPulses(pulses, millimetersPerPulse_);

  pulseCount_ = motion::saturatingAdd(pulseCount_, pulseIncrement);
  const int64_t magnitude =
      distanceIncrement >
              static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
          ? std::numeric_limits<int64_t>::max()
          : static_cast<int64_t>(distanceIncrement);
  distanceMillimeters_ = motion::saturatingAddSigned(
      distanceMillimeters_, reverseActive ? -magnitude : magnitude);
}

void TripCounter::reset() {
  distanceMillimeters_ = 0;
  pulseCount_ = 0;
}

}  // namespace domain
