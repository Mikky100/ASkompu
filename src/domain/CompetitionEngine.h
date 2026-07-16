#pragma once

#include <cstdint>

#include "RouteOrder.h"
#include "CompetitionRecords.h"

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
  JAT_COMPLETED,
  MITTIS_PROPOSAL,
};

struct MittisProposal {
  bool pending = false;
  bool valid = false;
  uint32_t oldMillimetersPerPulse = 0;
  uint32_t proposedMillimetersPerPulse = 0;
  int64_t measuredDistanceMillimeters = 0;
  uint32_t referenceDistanceMeters = 0;
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
  PointResult pointReleased(uint64_t monotonicMs,
                            uint32_t millimetersPerPulse = 0);
  PointResult pointReleased(uint64_t monotonicMs,
                            uint32_t millimetersPerPulse,
                            EventClockTime clockTime,
                            const CompetitionSettings& settings);
  PointResult resolveMittisProposal();
  void dismissJatResult(uint64_t monotonicMs);
  bool acceptNextStageStart(EventClockTime acceptedClockTime,
                            uint32_t nowMillisecondsOfDay,
                            uint64_t monotonicMs);
  bool beginStartTimeCorrection();
  void rejectStartTimeCorrection(uint64_t monotonicMs);
  bool applyOverride(const SegmentOverride& requested,
                     SegmentOverride& accepted);
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
  const MittisProposal& mittisProposal() const { return mittisProposal_; }
  uint16_t stageIndex() const { return stageIndex_; }
  const std::vector<StageResult>& stageResults() const { return stageResults_; }
  uint64_t totalPoints() const { return totalStagePoints(stageResults_); }
  EventClockTime arrivalClockTime() const { return arrivalClockTime_; }
  int64_t finalDeltaMs() const { return finalDeltaMs_; }
  EventClockTime proposedStartClockTime() const {
    return proposedStartClockTime_;
  }
  JatType pendingJatType() const { return pendingJatType_; }
  bool stageDistanceEnabled() const { return stageDistanceEnabled_; }
  bool correctingStartTime() const { return correctingStartTime_; }
  const SegmentOverride* activeOverride() const {
    return hasActiveOverride_ ? &activeOverride_ : nullptr;
  }
  uint16_t nextJatOrFinishPointIndex(uint16_t startSegmentIndex) const;

 private:
  int64_t activeSegmentIdealMs() const;
  PointResult completeCurrentPoint(uint64_t monotonicMs);
  PointResult finishJat(uint64_t monotonicMs,
                        const SegmentDefinition& segment);
  void beginNextStartTimeEdit();
  void updateCalculation(uint64_t monotonicMs);

  RouteOrder order_;
  CompetitionState state_ = CompetitionState::IDLE;
  bool active_ = false;
  uint16_t currentSegmentIndex_ = 0;
  uint16_t stageIndex_ = 0;
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
  CompetitionSettings settings_;
  EventClockTime pointClockTime_;
  EventClockTime arrivalClockTime_;
  EventClockTime proposedStartClockTime_;
  int64_t finalDeltaMs_ = 0;
  uint64_t jatResultUntilMonotonicMs_ = 0;
  JatType pendingJatType_ = JatType::MANNED_JAT;
  int16_t pendingJatOffsetMinutes_ = 0;
  bool stageDistanceEnabled_ = true;
  bool correctingStartTime_ = false;
  int64_t acceptedStartTimelineMs_ = 0;
  EventClockTime acceptedStartClockTime_;
  bool hasActiveOverride_ = false;
  SegmentOverride activeOverride_;
  int64_t overrideIdealBeforeMs_ = 0;
  std::vector<StageResult> stageResults_;
  MittisProposal mittisProposal_;
  int64_t distanceWhileMittisPendingMm_ = 0;
  PointUndoSnapshot undo_;
};

}  // namespace domain
