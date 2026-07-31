#pragma once

#include <cstdint>

namespace input {

constexpr uint32_t minimumPulseIntervalUs(uint32_t millimetersPerPulse,
                                          uint32_t maximumSpeedKmh) {
  return maximumSpeedKmh == 0
             ? 0
             : static_cast<uint32_t>(
                   (static_cast<uint64_t>(millimetersPerPulse) * 3600ULL +
                    maximumSpeedKmh - 1ULL) /
                   maximumSpeedKmh);
}

constexpr bool acceptsPulseInterval(uint32_t timestampUs,
                                    uint32_t lastAcceptedTimestampUs,
                                    uint32_t minimumIntervalUs,
                                    bool hasAcceptedPulse) {
  return !hasAcceptedPulse || minimumIntervalUs == 0 ||
         static_cast<uint32_t>(timestampUs - lastAcceptedTimestampUs) >=
             minimumIntervalUs;
}

}  // namespace input
