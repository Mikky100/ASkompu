#pragma once

#include <Arduino.h>

namespace domain {

class TripModel {
 public:
  void updateFromPulses(uint32_t pulseCount, uint32_t coefficient) {
    if (coefficient == 0) {
      tripMeters_ = 0;
      return;
    }
    tripMeters_ = (pulseCount * 1000UL) / coefficient;
  }

  void reset() { resetOffset_ = totalPulsesAtReset_; }

  void setTotalPulsesAtReset(uint32_t totalPulses) {
    totalPulsesAtReset_ = totalPulses;
    resetOffset_ = totalPulses;
  }

  uint32_t calculateEffectivePulses(uint32_t totalPulses) const {
    return (totalPulses >= resetOffset_) ? (totalPulses - resetOffset_) : 0;
  }

  uint32_t meters() const { return tripMeters_; }

 private:
  uint32_t tripMeters_ = 0;
  uint32_t totalPulsesAtReset_ = 0;
  uint32_t resetOffset_ = 0;
};

}  // namespace domain
