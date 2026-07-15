#include "MotionMath.h"

#include <limits>

namespace domain {
namespace motion {

uint64_t distanceMillimetersForPulses(uint32_t pulses,
                                      uint32_t millimetersPerPulse) {
  return static_cast<uint64_t>(pulses) *
         static_cast<uint64_t>(millimetersPerPulse);
}

uint64_t saturatingAdd(uint64_t current, uint64_t increment) {
  const uint64_t maximum = std::numeric_limits<uint64_t>::max();
  return increment > maximum - current ? maximum : current + increment;
}

float speedKmhFromPulseInterval(uint32_t millimetersPerPulse,
                                uint32_t intervalUs) {
  if (intervalUs == 0) {
    return 0.0F;
  }
  return static_cast<float>(millimetersPerPulse) * 3600.0F /
         static_cast<float>(intervalUs);
}

bool elapsedAtLeast(uint32_t now, uint32_t since, uint32_t duration) {
  return now - since >= duration;
}

}  // namespace motion
}  // namespace domain
