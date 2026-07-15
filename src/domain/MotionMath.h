#pragma once

#include <cstdint>

namespace domain {
namespace motion {

uint64_t distanceMillimetersForPulses(uint32_t pulses,
                                      uint32_t millimetersPerPulse);
uint64_t saturatingAdd(uint64_t current, uint64_t increment);
float speedKmhFromPulseInterval(uint32_t millimetersPerPulse,
                                uint32_t intervalUs);
bool elapsedAtLeast(uint32_t now, uint32_t since, uint32_t duration);

}  // namespace motion
}  // namespace domain
