#pragma once

#include <cstdint>

#ifdef ARDUINO
#include <Arduino.h>
#define ASKOMPU_PULSE_IRAM IRAM_ATTR
#else
#define ASKOMPU_PULSE_IRAM
#endif

namespace input {

struct PulseTrace {
  uint32_t timestampUs = 0;
  uint32_t observedIntervalUs = 0;
  uint32_t sinceAcceptedUs = 0;
  bool accepted = false;
};

struct PulseDiagnosticsSnapshot {
  uint32_t observed = 0;
  uint32_t accepted = 0;
  uint32_t rejected = 0;
  uint32_t delivered = 0;
  uint32_t observedIntervalUs = 0;
  uint32_t acceptedIntervalUs = 0;
  uint32_t minimumAcceptedUs = 0;
  uint32_t maximumAcceptedUs = 0;
  uint32_t lastAcceptedUs = 0;
  uint32_t minimumIntervalUs = 0;
  uint32_t historyOverwrites = 0;
  uint32_t historyNotRecorded = 0;
  uint32_t pendingOverflows = 0;
  uint32_t lifetimeOverflows = 0;
  bool counterOverflow = false;
  bool frozen = false;
  bool automaticTrigger = false;
  bool hasAccepted = false;
  uint8_t historySize = 0;
};

// Caller serializes all access. The ISR performs no allocation or I/O.
class PulseDiagnostics {
 public:
  static constexpr uint8_t CAPACITY = 32;

  void ASKOMPU_PULSE_IRAM observe(uint32_t now, bool accepted,
                                  bool hadAccepted,
                                  uint32_t lastAccepted) {
    increment(data_.observed);
    const uint32_t observedInterval = hasObserved_ ? now - lastObserved_ : 0;
    data_.observedIntervalUs = observedInterval;
    lastObserved_ = now;
    hasObserved_ = true;
    const uint32_t interval = hadAccepted ? now - lastAccepted : 0;
    bool suspicious = false;
    if (accepted) {
      increment(data_.accepted);
      if (hadAccepted) {
        suspicious = data_.acceptedIntervalUs > 0 &&
                     static_cast<uint64_t>(interval) * 10ULL >=
                         static_cast<uint64_t>(data_.acceptedIntervalUs) *
                             18ULL;
        data_.acceptedIntervalUs = interval;
        if (data_.minimumAcceptedUs == 0 ||
            interval < data_.minimumAcceptedUs)
          data_.minimumAcceptedUs = interval;
        if (interval > data_.maximumAcceptedUs)
          data_.maximumAcceptedUs = interval;
      }
      data_.lastAcceptedUs = now;
      data_.hasAccepted = true;
    } else {
      increment(data_.rejected);
    }
    if (!data_.frozen) {
      PulseTrace& sample = history_[next_];
      sample.timestampUs = now;
      sample.observedIntervalUs = observedInterval;
      sample.sinceAcceptedUs = interval;
      sample.accepted = accepted;
      next_ = static_cast<uint8_t>((next_ + 1U) % CAPACITY);
      if (data_.historySize < CAPACITY)
        ++data_.historySize;
      else
        increment(data_.historyOverwrites);
      if (suspicious) {
        data_.frozen = true;
        data_.automaticTrigger = true;
      }
    } else {
      increment(data_.historyNotRecorded);
    }
  }

  void delivered(uint32_t count) {
    if (UINT32_MAX - data_.delivered < count) {
      data_.delivered = UINT32_MAX;
      data_.counterOverflow = true;
    } else {
      data_.delivered += count;
    }
  }
  void ASKOMPU_PULSE_IRAM pendingOverflow() {
    increment(data_.pendingOverflows);
  }
  void ASKOMPU_PULSE_IRAM lifetimeOverflow() {
    increment(data_.lifetimeOverflows);
  }
  void freeze() { data_.frozen = true; }
  void reset() {
    data_ = PulseDiagnosticsSnapshot{};
    hasObserved_ = false;
    next_ = 0;
  }
  PulseDiagnosticsSnapshot snapshot(uint32_t minimumInterval) const {
    PulseDiagnosticsSnapshot copy = data_;
    copy.minimumIntervalUs = minimumInterval;
    return copy;
  }
  PulseTrace trace(uint8_t index) const {
    if (index >= data_.historySize) return {};
    const uint8_t first = data_.historySize == CAPACITY ? next_ : 0;
    return history_[(first + index) % CAPACITY];
  }

 private:
  void ASKOMPU_PULSE_IRAM increment(uint32_t& count) {
    if (count == UINT32_MAX)
      data_.counterOverflow = true;
    else
      ++count;
  }

  PulseDiagnosticsSnapshot data_{};
  PulseTrace history_[CAPACITY]{};
  uint32_t lastObserved_ = 0;
  bool hasObserved_ = false;
  uint8_t next_ = 0;
};

}  // namespace input
