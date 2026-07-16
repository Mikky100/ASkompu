#pragma once

#include <cstdint>

#include "RouteOrder.h"

namespace domain {

enum class CompetitionState : uint8_t {
  IDLE,
  WAIT_START,
  RUNNING,
  JAT_RESULT,
  EDIT_START_TIME,
  FINISHED,
};

enum class PointResult : uint8_t {
  IGNORED,
  ADVANCED,
  FINISHED,
  JAT_NOT_IMPLEMENTED,
};

struct PointUndoSnapshot {
  uint16_t segmentIndex = 0;
  int64_t segmentDistanceMillimeters = 0;
  int64_t idealBeforeSegmentMs = 0;
  uint64_t deadlineMonotonicMs = 0;
  bool valid = false;
};

// Hardware-independent competition runtime. Persistent event logging and
// crash-safe runtime restoration are intentionally outside this first phase.
class CompetitionEngine {
 public:
  static constexpr uint32_t POINT_UNDO_WINDOW_MS = 3000;

  bool activate(const RouteOrder& order, uint32_t nowMillisecondsOfDay,
                uint64_t monotonicMs);
  void clear();
  void tick(uint64_t monotonicMs);
  void addDistanceMillimeters(int64_t deltaMillimeters);
  PointResult pointReleased(uint64_t monotonicMs);
  bool undoPoint(uint64_t monotonicMs);

  CompetitionState state() const { return state_; }
  const RouteOrder* routeOrder() const { return active_ ? &order_ : nullptr; }
  uint16_t currentSegmentIndex() const { return currentSegmentIndex_; }
  const SegmentDefinition* currentSegment() const;
  const SegmentDefinition* nextSegment() const;
  int64_t realTimeMs() const { return realTimeMs_; }
  int64_t idealTimeMs() const { return idealTimeMs_; }
  int64_t deltaMs() const { return deltaMs_; }
  int64_t deltaSeconds() const { return deltaMs_ / 1000; }
  int64_t physicalDistanceMillimeters() const { return physicalDistanceMm_; }
  int64_t stageDistanceMillimeters() const { return stageDistanceMm_; }
  int64_t segmentDistanceMillimeters() const { return segmentDistanceMm_; }
  bool reverseActive() const { return reverseActive_; }
  void setReverseActive(bool active) { reverseActive_ = active; }
  bool deltaFrozen() const { return deltaFrozen_; }
  bool hasUndoPrompt(uint64_t monotonicMs) const;
  bool jatNotImplemented() const { return jatNotImplemented_; }

 private:
  int64_t activeSegmentIdealMs() const;
  void updateCalculation(uint64_t monotonicMs);

  RouteOrder order_;
  CompetitionState state_ = CompetitionState::IDLE;
  bool active_ = false;
  uint16_t currentSegmentIndex_ = 0;
  int64_t startTimelineMs_ = 0;
  uint64_t lastMonotonicMs_ = 0;
  int64_t realTimeMs_ = 0;
  int64_t idealBeforeSegmentMs_ = 0;
  int64_t idealTimeMs_ = 0;
  int64_t deltaMs_ = 0;
  int64_t physicalDistanceMm_ = 0;
  int64_t stageDistanceMm_ = 0;
  int64_t segmentDistanceMm_ = 0;
  bool reverseActive_ = false;
  bool deltaFrozen_ = false;
  bool jatNotImplemented_ = false;
  PointUndoSnapshot undo_;
};

}  // namespace domain
