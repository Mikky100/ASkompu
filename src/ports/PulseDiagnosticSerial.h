#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdio>

#include "input/PulseInput.h"

namespace ports {

class PulseDiagnosticSerial {
 public:
  void cancel() {
    length_ = offset_ = 0;
    nextSample_ = count_ = 0;
  }

  void capture(input::PulseInput& pulses, uint32_t mmPerPulse, uint8_t pin,
               bool falling) {
    cancel();
    pulses.freezeDiagnostics();
    const input::PulseDiagnosticsSnapshot snapshot = pulses.diagnostics();
    const uint32_t nowUs = micros();
    count_ = snapshot.historySize;
    for (uint8_t i = 0; i < count_; ++i) samples_[i] = pulses.trace(i);
    length_ = std::snprintf(
        buffer_, sizeof(buffer_),
        "PULSE gpio=%u edge=%s mm/pulse=%lu min_us=%lu\n"
        "observed=%lu accepted=%lu rejected=%lu delivered=%lu pending=%lu\n"
        "observed_dt_us=%lu accepted_dt_us=%lu age_us=%lu hz=%.3f kmh=%.3f\n"
        "shortest_us=%lu longest_us=%lu counter_overflow=%u "
        "pending_overflow=%lu lifetime_overflow=%lu\n"
        "history_overwritten=%lu history_not_recorded=%lu frozen=%u "
        "auto_hint=%u\n"
        "timestamp_us,observed_dt_us,since_accepted_us,accepted\n",
        pin, falling ? "FALLING" : "RISING",
        static_cast<unsigned long>(mmPerPulse),
        static_cast<unsigned long>(snapshot.minimumIntervalUs),
        static_cast<unsigned long>(snapshot.observed),
        static_cast<unsigned long>(snapshot.accepted),
        static_cast<unsigned long>(snapshot.rejected),
        static_cast<unsigned long>(snapshot.delivered),
        static_cast<unsigned long>(snapshot.accepted - snapshot.delivered),
        static_cast<unsigned long>(snapshot.observedIntervalUs),
        static_cast<unsigned long>(snapshot.acceptedIntervalUs),
        static_cast<unsigned long>(snapshot.hasAccepted
                                       ? nowUs - snapshot.lastAcceptedUs
                                       : 0),
        snapshot.acceptedIntervalUs
            ? 1000000.0 / snapshot.acceptedIntervalUs
            : 0.0,
        snapshot.acceptedIntervalUs
            ? mmPerPulse * 3600.0 / snapshot.acceptedIntervalUs
            : 0.0,
        static_cast<unsigned long>(snapshot.minimumAcceptedUs),
        static_cast<unsigned long>(snapshot.maximumAcceptedUs),
        snapshot.counterOverflow,
        static_cast<unsigned long>(snapshot.pendingOverflows),
        static_cast<unsigned long>(snapshot.lifetimeOverflows),
        static_cast<unsigned long>(snapshot.historyOverwrites),
        static_cast<unsigned long>(snapshot.historyNotRecorded),
        snapshot.frozen, snapshot.automaticTrigger);
    if (length_ >= sizeof(buffer_)) length_ = sizeof(buffer_) - 1;
  }

  void service() {
    if (!Serial) return;
    if (offset_ == length_ && nextSample_ < count_) {
      const input::PulseTrace& sample = samples_[nextSample_++];
      length_ = std::snprintf(
          buffer_, sizeof(buffer_), "%lu,%lu,%lu,%u\n",
          static_cast<unsigned long>(sample.timestampUs),
          static_cast<unsigned long>(sample.observedIntervalUs),
          static_cast<unsigned long>(sample.sinceAcceptedUs), sample.accepted);
      offset_ = 0;
    }
    if (offset_ >= length_) return;
    const int room = Serial.availableForWrite();
    if (room <= 0) return;
    size_t bytes = length_ - offset_;
    if (bytes > 64) bytes = 64;
    if (bytes > static_cast<size_t>(room)) bytes = room;
    offset_ += Serial.write(
        reinterpret_cast<const uint8_t*>(buffer_ + offset_), bytes);
  }

 private:
  char buffer_[768]{};
  input::PulseTrace samples_[input::PulseDiagnostics::CAPACITY]{};
  size_t length_ = 0;
  size_t offset_ = 0;
  uint8_t nextSample_ = 0;
  uint8_t count_ = 0;
};

}  // namespace ports
