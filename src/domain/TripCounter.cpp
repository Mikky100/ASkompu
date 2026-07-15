#include "TripCounter.h"

#include <limits>

namespace domain {

void TripCounter::incrementTestStep() {
  addTestSteps(1);
}

void TripCounter::addTestSteps(uint32_t steps) {
  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  value_ = steps > maximum - value_ ? maximum : value_ + steps;
}

void TripCounter::reset() {
  value_ = 0;
}

}  // namespace domain
