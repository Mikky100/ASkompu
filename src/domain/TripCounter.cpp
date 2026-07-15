#include "TripCounter.h"

#include <limits>

namespace domain {

TripCounter::TripCounter(uint32_t millimetersPerPulse)
    : millimetersPerPulse_(millimetersPerPulse) {}

void TripCounter::addPulses(uint32_t pulses) {
  const uint64_t maximum = std::numeric_limits<uint64_t>::max();
  const uint64_t pulseIncrement = pulses;
  const uint64_t distanceIncrement =
      pulseIncrement * static_cast<uint64_t>(millimetersPerPulse_);

  pulseCount_ = pulseIncrement > maximum - pulseCount_
                    ? maximum
                    : pulseCount_ + pulseIncrement;
  distanceMillimeters_ = distanceIncrement > maximum - distanceMillimeters_
                             ? maximum
                             : distanceMillimeters_ + distanceIncrement;
}

void TripCounter::reset() {
  distanceMillimeters_ = 0;
  pulseCount_ = 0;
}

}  // namespace domain
