#include "TripCounter.h"

#include "MotionMath.h"

namespace domain {

TripCounter::TripCounter(uint32_t millimetersPerPulse)
    : millimetersPerPulse_(millimetersPerPulse) {}

void TripCounter::setMillimetersPerPulse(uint32_t millimetersPerPulse) {
  millimetersPerPulse_ = millimetersPerPulse;
}

void TripCounter::addPulses(uint32_t pulses) {
  const uint64_t pulseIncrement = pulses;
  const uint64_t distanceIncrement =
      motion::distanceMillimetersForPulses(pulses, millimetersPerPulse_);

  pulseCount_ = motion::saturatingAdd(pulseCount_, pulseIncrement);
  distanceMillimeters_ =
      motion::saturatingAdd(distanceMillimeters_, distanceIncrement);
}

void TripCounter::reset() {
  distanceMillimeters_ = 0;
  pulseCount_ = 0;
}

}  // namespace domain
