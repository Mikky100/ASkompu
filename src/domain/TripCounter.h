#pragma once

#include <cstdint>

namespace domain {

class TripCounter {
 public:
  explicit TripCounter(uint32_t millimetersPerPulse);

  void setMillimetersPerPulse(uint32_t millimetersPerPulse);
  void addPulses(uint32_t pulses, bool reverseActive = false);
  void reset();
  int64_t distanceMillimeters() const { return distanceMillimeters_; }
  uint64_t pulseCount() const { return pulseCount_; }

 private:
  uint32_t millimetersPerPulse_;
  int64_t distanceMillimeters_ = 0;
  uint64_t pulseCount_ = 0;
};

}  // namespace domain
