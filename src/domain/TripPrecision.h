#pragma once
#include <cstdint>

namespace domain {
// Display-only hysteresis. The pulse-derived speed is the current speed source;
// no additional speed filter or distance quantization is introduced here.
class TripPrecision {
 public:
  void update(float speedKmh, uint32_t nowMs) {
    const bool candidate = tenMeters_ ? speedKmh < 29.0F : speedKmh > 31.0F;
    if (!candidate) { timing_ = false; return; }
    if (!timing_) { timing_ = true; sinceMs_ = nowMs; }
    if (static_cast<uint32_t>(nowMs - sinceMs_) >= 1500) {
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
