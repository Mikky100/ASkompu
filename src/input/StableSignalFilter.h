#pragma once

#include <cstdint>

namespace input {

// Continuous level input filter. A new state must remain unchanged for the
// configured interval. Unlike a button interpreter this emits no press events.
class StableSignalFilter {
 public:
  explicit StableSignalFilter(uint32_t stabilityMs)
      : stabilityMs_(stabilityMs) {}

  void reset(bool active, uint32_t nowMs) {
    rawActive_ = stableActive_ = active;
    rawChangedAtMs_ = nowMs;
    changed_ = false;
  }

  bool update(bool rawActive, uint32_t nowMs) {
    changed_ = false;
    if (rawActive != rawActive_) {
      rawActive_ = rawActive;
      rawChangedAtMs_ = nowMs;
    }
    if (rawActive_ != stableActive_ &&
        nowMs - rawChangedAtMs_ >= stabilityMs_) {
      stableActive_ = rawActive_;
      changed_ = true;
    }
    return changed_;
  }

  bool active() const { return stableActive_; }
  bool changed() const { return changed_; }

 private:
  const uint32_t stabilityMs_;
  bool rawActive_ = false;
  bool stableActive_ = false;
  bool changed_ = false;
  uint32_t rawChangedAtMs_ = 0;
};

}  // namespace input
