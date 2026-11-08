#pragma once

#include <cstdint>

namespace domain {

// Display-only hysteresis. The pulse-derived speed remains the speed source;
// this only controls whether the trip is rendered to 1 m or 10 m precision.
class TripPrecision {
 public:
  void update(float speedKmh, uint32_t nowMs) {
    const bool candidate = tenMeters_ ? speedKmh < 29.0F : speedKmh > 31.0F;
    if (!candidate) {
      timing_ = false;
      return;
    }
    if (!timing_) {
      timing_ = true;
      sinceMs_ = nowMs;
    }
    if (static_cast<uint32_t>(nowMs - sinceMs_) >= 1500U) {
      tenMeters_ = !tenMeters_;
      timing_ = false;
    }
  }

  bool tenMeters() const { return tenMeters_; }

 private:
  bool tenMeters_ = false;
  bool timing_ = false;
  uint32_t sinceMs_ = 0;
};

}  // namespace domain
