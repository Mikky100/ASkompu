#pragma once

#include <cstdint>

namespace core {

struct ClockTime {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

class TimeSource {
 public:
  virtual ~TimeSource() {}
  virtual uint32_t monotonicMilliseconds() const = 0;
};

class Clock {
 public:
  virtual ~Clock() {}
  virtual void set(uint8_t hour, uint8_t minute, uint8_t second) = 0;
  virtual bool isSet() const = 0;
  virtual ClockTime now() const = 0;
  virtual uint64_t elapsedSinceSetMilliseconds() const = 0;
};

class SoftwareClock : public Clock {
 public:
  explicit SoftwareClock(const TimeSource& timeSource);

  void set(uint8_t hour, uint8_t minute, uint8_t second) override;
  bool isSet() const override { return set_; }
  ClockTime now() const override;
  uint64_t elapsedSinceSetMilliseconds() const override;

 private:
  void update() const;

  const TimeSource& timeSource_;
  mutable bool set_ = false;
  mutable uint32_t lastMonotonicMs_ = 0;
  mutable uint64_t elapsedMs_ = 0;
  uint32_t setSecondsOfDay_ = 0;
};

bool isValidClockTime(uint8_t hour, uint8_t minute);

}  // namespace core
