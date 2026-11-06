#pragma once
#include <Arduino.h>
#include <cstdio>
#include "input/PulseInput.h"

namespace ports {
// Immutable report copied once; at most 64 bytes per loop, only with USB room.
// A disconnected host neither blocks nor grows a queue. A new request replaces
// the previous report. Reset cancels it so periods cannot get mixed.
class PulseDiagnosticSerial {
 public:
  void cancel() { length_ = offset_ = 0; nextSample_ = count_ = 0; }
  void capture(input::PulseInput& pulses, uint32_t mmPerPulse,
                uint8_t pin, bool falling) {
    cancel();
    pulses.freezeDiagnostics();
    const auto s = pulses.diagnostics();
    const uint32_t nowUs = micros();
    count_ = s.historySize;
    for (uint8_t i = 0; i < count_; ++i) samples_[i] = pulses.trace(i);
    length_ = std::snprintf(buffer_, sizeof(buffer_),
        "PULSE gpio=%u edge=%s mm/pulse=%lu min_us=%lu\n"
        "observed=%lu accepted=%lu rejected=%lu delivered=%lu pending=%lu\n"
        "observed_dt_us=%lu accepted_dt_us=%lu age_us=%lu hz=%.3f kmh=%.3f\n"
        "shortest_us=%lu longest_us=%lu counter_overflow=%u pending_overflow=%lu lifetime_overflow=%lu\n"
        "history_overwritten=%lu history_not_recorded=%lu frozen=%u auto_hint=%u\n"
        "timestamp_us,observed_dt_us,since_accepted_us,accepted\n",
        pin, falling ? "FALLING" : "RISING", (unsigned long)mmPerPulse,
        (unsigned long)s.minimumIntervalUs, (unsigned long)s.observed,
        (unsigned long)s.accepted, (unsigned long)s.rejected,
        (unsigned long)s.delivered, (unsigned long)(s.accepted - s.delivered),
        (unsigned long)s.observedIntervalUs, (unsigned long)s.acceptedIntervalUs,
        (unsigned long)(s.hasAccepted ? nowUs - s.lastAcceptedUs : 0),
        s.acceptedIntervalUs ? 1000000.0 / s.acceptedIntervalUs : 0.0,
        s.acceptedIntervalUs ? mmPerPulse * 3600.0 / s.acceptedIntervalUs : 0.0,
        (unsigned long)s.minimumAcceptedUs, (unsigned long)s.maximumAcceptedUs,
        s.counterOverflow, (unsigned long)s.pendingOverflows,
        (unsigned long)s.lifetimeOverflows, (unsigned long)s.historyOverwrites,
        (unsigned long)s.historyNotRecorded, s.frozen, s.automaticTrigger);
    if (length_ >= sizeof(buffer_)) length_ = sizeof(buffer_) - 1;
  }
  void service() {
    if (!Serial) return;
    if (offset_ == length_ && nextSample_ < count_) {
      const auto& s = samples_[nextSample_++];
      length_ = std::snprintf(buffer_, sizeof(buffer_), "%lu,%lu,%lu,%u\n",
          (unsigned long)s.timestampUs, (unsigned long)s.observedIntervalUs,
          (unsigned long)s.sinceAcceptedUs, s.accepted);
      offset_ = 0;
    }
    if (offset_ >= length_) return;
    const int room = Serial.availableForWrite();
    if (room <= 0) return;
    size_t bytes = length_ - offset_;
    if (bytes > 64) bytes = 64;
    if (bytes > static_cast<size_t>(room)) bytes = room;
    offset_ += Serial.write(reinterpret_cast<const uint8_t*>(buffer_ + offset_), bytes);
  }
 private:
  char buffer_[768]{};
  input::PulseTrace samples_[input::PulseDiagnostics::CAPACITY];
  size_t length_ = 0, offset_ = 0;
  uint8_t nextSample_ = 0, count_ = 0;
};
}  // namespace ports
