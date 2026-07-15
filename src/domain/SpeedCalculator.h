#pragma once

#include <cstdint>

namespace domain {

class SpeedCalculator {
 public:
  SpeedCalculator(uint32_t millimetersPerPulse,
                  uint32_t zeroSpeedTimeoutUs);

  void setMillimetersPerPulse(uint32_t millimetersPerPulse);
  void update(uint32_t nowUs, uint32_t totalPulses,
              uint32_t previousPulseAtUs, uint32_t lastPulseAtUs);
  float speedKmh() const { return speedKmh_; }

 private:
  uint32_t millimetersPerPulse_;
  const uint32_t zeroSpeedTimeoutUs_;
  uint32_t calculatedAtPulseUs_ = 0;
  float speedKmh_ = 0.0F;
};

}  // namespace domain
