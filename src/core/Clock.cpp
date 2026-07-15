#include "Clock.h"

namespace core {

namespace {
constexpr uint32_t SECONDS_PER_DAY = 24UL * 60UL * 60UL;
}

SoftwareClock::SoftwareClock(const TimeSource& timeSource)
    : timeSource_(timeSource) {}

void SoftwareClock::set(uint8_t hour, uint8_t minute, uint8_t second) {
  if (!isValidClockTime(hour, minute) || second > 59) {
    return;
  }
  setSecondsOfDay_ = static_cast<uint32_t>(hour) * 3600UL +
                     static_cast<uint32_t>(minute) * 60UL + second;
  elapsedMs_ = 0;
  lastMonotonicMs_ = timeSource_.monotonicMilliseconds();
  set_ = true;
}

ClockTime SoftwareClock::now() const {
  if (!set_) {
    return {0, 0, 0};
  }
  update();
  const uint32_t secondsOfDay =
      static_cast<uint32_t>((setSecondsOfDay_ + elapsedMs_ / 1000ULL) %
                            SECONDS_PER_DAY);
  return {static_cast<uint8_t>(secondsOfDay / 3600UL),
          static_cast<uint8_t>((secondsOfDay / 60UL) % 60UL),
          static_cast<uint8_t>(secondsOfDay % 60UL)};
}

uint64_t SoftwareClock::elapsedSinceSetMilliseconds() const {
  if (!set_) {
    return 0;
  }
  update();
  return elapsedMs_;
}

void SoftwareClock::update() const {
  const uint32_t current = timeSource_.monotonicMilliseconds();
  elapsedMs_ += static_cast<uint32_t>(current - lastMonotonicMs_);
  lastMonotonicMs_ = current;
}

bool isValidClockTime(uint8_t hour, uint8_t minute) {
  return hour <= 23 && minute <= 59;
}

}  // namespace core
