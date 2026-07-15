#include "TripCounter.h"

#include <limits>

namespace domain {

void TripCounter::incrementTestStep() {
  if (value_ < std::numeric_limits<uint32_t>::max()) {
    ++value_;
  }
}

void TripCounter::reset() {
  value_ = 0;
}

}  // namespace domain

