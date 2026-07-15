#pragma once

#include <cstdint>

namespace domain {

class TripCounter {
 public:
  void incrementTestStep();
  void reset();
  uint32_t value() const { return value_; }

 private:
  uint32_t value_ = 0;
};

}  // namespace domain

