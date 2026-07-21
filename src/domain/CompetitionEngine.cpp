#include "CompetitionEngine.h"

#include <limits>

namespace domain {
namespace {

constexpr int64_t DAY_MS = 24LL * 60LL * 60LL * 1000LL;
constexpr int64_t HALF_DAY_MS = DAY_MS / 2;

uint32_t secondsOfDay(EventClockTime value) {
  return static_cast<uint32_t>(value.hour) * 3600UL +
         static_cast<uint32_t>(value.minute) * 60UL + value.second;
}

EventClockTime clockFromSeconds(int64_t seconds) {
  constexpr int64_t DAY_SECONDS = 24LL * 60LL * 60LL;
  seconds %= DAY_SECONDS;
  if (seconds < 0) seconds += DAY_SECONDS;
  return {static_cast<uint8_t>(seconds / 3600),
          static_cast<uint8_t>((seconds / 60) % 60),
          static_cast<uint8_t>(seconds % 60)};
}

EventClockTime roundedMinuteProposal(EventClockTime arrival) {
  const uint32_t arrivalSeconds = secondsOfDay(arrival);
  uint32_t proposal = (arrivalSeconds / 60UL + 1UL) * 60UL;
  if (proposal - arrivalSeconds < 20UL) proposal += 60UL;
  return clockFromSeconds(proposal);
}

int64_t saturatingToInt64(uint64_t value) {
  return value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
             ? std::numeric_limits<int64_t>::max()
             : static_cast<int64_t>(value);
}

int64_t speedIdealMilliseconds(int64_t distanceMm, uint32_t speedKmh) {
  if (speedKmh == 0) return 0;
  // distance_m * 3.6 / km/h converted to ms:
  // distance_mm * 36 / (10 * km/h). Split before multiplication.
  const bool negative = distanceMm < 0;
  const uint64_t magnitude = negative
                                 ? static_cast<uint64_t>(-(distanceMm + 1)) + 1
                                 : static_cast<uint64_t>(distanceMm);
  const uint64_t divisor = static_cast<uint64_t>(speedKmh) * 10ULL;
  const uint64_t whole = magnitude / divisor;
  const uint64_t remainder = magnitude % divisor;
  const uint64_t fractional = remainder * 36ULL / divisor;
  const uint64_t limit =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) +
      (negative ? 1ULL : 0ULL);
  if (whole > limit / 36ULL ||
      (whole == limit / 36ULL && fractional > limit % 36ULL)) {
    return negative ? std::numeric_limits<int64_t>::min()
                    : std::numeric_limits<int64_t>::max();
  }
  const uint64_t result = whole * 36ULL + fractional;
  if (negative) {
    if (result >= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1)
      return std::numeric_limits<int64_t>::min();
    return -static_cast<int64_t>(result);
  }
  return static_cast<int64_t>(result);
}

int64_t saturatingAddInt64(int64_t left, int64_t right) {
  if (right > 0 && left > std::numeric_limits<int64_t>::max() - right)
    return std::numeric_limits<int64_t>::max();
  if (right < 0 && left < std::numeric_limits<int64_t>::min() - right)
    return std::numeric_limits<int64_t>::min();
  return left + right;
}

}  // namespace

bool CompetitionEngine::activate(const RouteOrder& order,
                                 uint32_t nowMillisecondsOfDay,
                                 uint64_t monotonicMs) {
  if (validateRouteOrder(order) != RouteOrderValidationError::NONE ||
      nowMillisecondsOfDay >= DAY_MS) {
    return false;
  }
  order_ = order;
  active_ = true;
  state_ = CompetitionState::WAIT_START;
  currentSegmentIndex_ = 0;
  stageIndex_ = 0;
  lastMonotonicMs_ = monotonicMs;
  const int64_t startOfDayMs =
      (static_cast<int64_t>(order.startHour) * 60 + order.startMinute) * 60000;
  int64_t untilStartMs = startOfDayMs - nowMillisecondsOfDay;
  if (untilStartMs > HALF_DAY_MS) untilStartMs -= DAY_MS;
  if (untilStartMs < -HALF_DAY_MS) untilStartMs += DAY_MS;
  startTimelineMs_ = saturatingAddInt64(saturatingToInt64(monotonicMs),
                                        untilStartMs);
  acceptedStartTimelineMs_ = startTimelineMs_;
  acceptedStartClockTime_ = {order.startHour, order.startMinute, 0};
  realTimeMs_ = -untilStartMs;
  idealBeforeSegmentMs_ = 0;
  physicalDistanceMm_ = stageDistanceMm_ = segmentDistanceMm_ = 0;
  reverseActive_ = false;
  deltaFrozen_ = false;
  finalDeltaMs_ = 0;
  stageDistanceEnabled_ = true;
  correctingStartTime_ = false;
  stageResults_.clear();
  hasActiveOverride_ = false;
  mittisProposal_ = MittisProposal{};
  distanceWhileMittisPendingMm_ = 0;
  undo_ = PointUndoSnapshot{};
  updateCalculation(monotonicMs);
  return true;
}

void CompetitionEngine::clear() { *this = CompetitionEngine{}; }

const SegmentDefinition* CompetitionEngine::currentSegment() const {
  return active_ && currentSegmentIndex_ < order_.segments.size()
             ? &order_.segments[currentSegmentIndex_]
             : nullptr;
}

const SegmentDefinition* CompetitionEngine::nextSegment() const {
  return active_ && currentSegmentIndex_ + 1 < order_.segments.size()
             ? &order_.segments[currentSegmentIndex_ + 1]
             : nullptr;
}

int64_t CompetitionEngine::activeSegmentIdealMs() const {
  const SegmentDefinition* segment = currentSegment();
  if (!segment) return 0;
  if (hasActiveOverride_ &&
      currentSegmentIndex_ >= activeOverride_.startSegmentIndex &&
      currentSegmentIndex_ < activeOverride_.endPointIndex)
    return static_cast<int64_t>(activeOverride_.replacementDurationSeconds) *
           1000LL;
  if (segment->segmentType == SegmentType::TIME)
    return static_cast<int64_t>(segment->value) * 1000LL;
  if (segment->segmentType == SegmentType::SPEED)
    return speedIdealMilliseconds(segmentDistanceMm_, segment->value);
  if (segment->segmentType == SegmentType::MITTIS)
    return static_cast<int64_t>(segment->mittisDurationSeconds) * 1000LL;
  return 0;
}

void CompetitionEngine::updateCalculation(uint64_t monotonicMs) {
  if (!active_ || state_ == CompetitionState::FINISHED ||
      state_ == CompetitionState::JAT_RESULT ||
      state_ == CompetitionState::EDIT_START_TIME)
    return;
  lastMonotonicMs_ = monotonicMs;
  if (mittisProposal_.pending) return;
  realTimeMs_ = saturatingAddInt64(saturatingToInt64(monotonicMs),
                                   -startTimelineMs_);
  if (state_ == CompetitionState::WAIT_START) {
    if (realTimeMs_ >= 0) {
      state_ = CompetitionState::RUNNING;
    } else if (stageIndex_ > 0) {
      // A stage after JAT has no running competition clock before its accepted
      // start. The completed stage result remains in finalDeltaMs_.
      realTimeMs_ = 0;
      idealTimeMs_ = 0;
      deltaMs_ = 0;
      return;
    }
  }
  idealTimeMs_ = saturatingAddInt64(idealBeforeSegmentMs_,
                                    activeSegmentIdealMs());
  deltaMs_ = saturatingAddInt64(idealTimeMs_, -realTimeMs_);
}

void CompetitionEngine::tick(uint64_t monotonicMs) {
  if (state_ == CompetitionState::JAT_RESULT &&
      monotonicMs >= jatResultUntilMonotonicMs_) {
    beginNextStartTimeEdit();
  }
  updateCalculation(monotonicMs);
  if (undo_.valid && monotonicMs > undo_.deadlineMonotonicMs) undo_.valid = false;
}

void CompetitionEngine::addDistanceMillimeters(int64_t deltaMillimeters) {
  if (!active_ || state_ == CompetitionState::FINISHED) return;
  physicalDistanceMm_ = saturatingAddInt64(physicalDistanceMm_, deltaMillimeters);
  if (!stageDistanceEnabled_) return;
  stageDistanceMm_ = saturatingAddInt64(stageDistanceMm_, deltaMillimeters);
  if (mittisProposal_.pending) {
    distanceWhileMittisPendingMm_ =
        saturatingAddInt64(distanceWhileMittisPendingMm_, deltaMillimeters);
    return;
  }
  segmentDistanceMm_ = saturatingAddInt64(segmentDistanceMm_, deltaMillimeters);
  updateCalculation(lastMonotonicMs_);
}

PointResult CompetitionEngine::pointReleased(uint64_t monotonicMs,
                                             uint32_t millimetersPerPulse) {
  return pointReleased(monotonicMs, millimetersPerPulse, EventClockTime{},
                       CompetitionSettings{});
}

PointResult CompetitionEngine::pointReleased(
    uint64_t monotonicMs, uint32_t millimetersPerPulse,
    EventClockTime clockTime, const CompetitionSettings& settings) {
  if (state_ != CompetitionState::RUNNING) return PointResult::IGNORED;
  if (mittisProposal_.pending) return PointResult::IGNORED;
  pointClockTime_ = clockTime;
  settings_ = validatedCompetitionSettings(settings);
  updateCalculation(monotonicMs);
  const SegmentDefinition* segment = currentSegment();
  if (!segment) return PointResult::IGNORED;
  if (segment->segmentType == SegmentType::MITTIS) {
    mittisProposal_.pending = true;
    mittisProposal_.oldMillimetersPerPulse = millimetersPerPulse;
    mittisProposal_.measuredDistanceMillimeters = segmentDistanceMm_;
    mittisProposal_.referenceDistanceMeters = segment->value;
    distanceWhileMittisPendingMm_ = 0;
    if (millimetersPerPulse > 0 && segmentDistanceMm_ > 0) {
      const uint64_t numerator =
          static_cast<uint64_t>(millimetersPerPulse) * segment->value * 1000ULL;
      const uint64_t proposed =
          numerator / static_cast<uint64_t>(segmentDistanceMm_);
      if (proposed > 0 && proposed <= std::numeric_limits<uint32_t>::max()) {
        mittisProposal_.valid = true;
        mittisProposal_.proposedMillimetersPerPulse =
            static_cast<uint32_t>(proposed);
      }
    }
    undo_.valid = false;
    return PointResult::MITTIS_PROPOSAL;
  }
  return completeCurrentPoint(monotonicMs);
}

PointResult CompetitionEngine::completeCurrentPoint(uint64_t monotonicMs) {
  const SegmentDefinition* segment = currentSegment();
  if (!segment) return PointResult::IGNORED;
  if (segment->pointTypeAtEnd == PointType::JAT) {
    return finishJat(monotonicMs, *segment);
  }
  if (segment->pointTypeAtEnd == PointType::FINISH_M) {
    finalDeltaMs_ = deltaMs_;
    stageResults_.push_back(scoreStage(stageIndex_, PointType::FINISH_M,
                                       pointClockTime_, finalDeltaMs_,
                                       settings_.lateFactor,
                                       settings_.earlyFactor));
    deltaFrozen_ = true;
    state_ = CompetitionState::FINISHED;
    undo_.valid = false;
    return PointResult::FINISHED;
  }
  if (currentSegmentIndex_ + 1 >= order_.segments.size())
    return PointResult::IGNORED;
  if (hasActiveOverride_ &&
      currentSegmentIndex_ >= activeOverride_.startSegmentIndex &&
      currentSegmentIndex_ + 1 < activeOverride_.endPointIndex) {
    ++currentSegmentIndex_;
    idealBeforeSegmentMs_ = overrideIdealBeforeMs_;
    segmentDistanceMm_ = distanceWhileMittisPendingMm_;
    distanceWhileMittisPendingMm_ = 0;
    undo_.valid = false;
    updateCalculation(monotonicMs);
    return PointResult::ADVANCED;
  }
  undo_.segmentIndex = currentSegmentIndex_;
  undo_.segmentDistanceMillimeters = segmentDistanceMm_;
  undo_.idealBeforeSegmentMs = idealBeforeSegmentMs_;
  undo_.deadlineMonotonicMs = monotonicMs + POINT_UNDO_WINDOW_MS;
  undo_.valid = true;
  idealBeforeSegmentMs_ = idealTimeMs_;
  ++currentSegmentIndex_;
  if (hasActiveOverride_ &&
      currentSegmentIndex_ == activeOverride_.startSegmentIndex)
    overrideIdealBeforeMs_ = idealBeforeSegmentMs_;
  segmentDistanceMm_ = distanceWhileMittisPendingMm_;
  distanceWhileMittisPendingMm_ = 0;
  if (hasActiveOverride_ &&
      currentSegmentIndex_ >= activeOverride_.endPointIndex)
    hasActiveOverride_ = false;
  updateCalculation(monotonicMs);
  return PointResult::ADVANCED;
}

PointResult CompetitionEngine::finishJat(
    uint64_t monotonicMs, const SegmentDefinition& segment) {
  finalDeltaMs_ = deltaMs_;
  arrivalClockTime_ = pointClockTime_;
  pendingJatType_ = segment.jatType;
  pendingJatOffsetMinutes_ =
      segment.hasJatOffsetMinutes ? segment.jatOffsetMinutes : 0;
  stageResults_.push_back(scoreStage(stageIndex_, PointType::JAT,
                                     arrivalClockTime_, finalDeltaMs_,
                                     settings_.lateFactor,
                                     settings_.earlyFactor));
  deltaFrozen_ = true;
  undo_.valid = false;
  if (currentSegmentIndex_ + 1 < order_.segments.size())
    ++currentSegmentIndex_;
  ++stageIndex_;
  idealBeforeSegmentMs_ = 0;
  idealTimeMs_ = 0;
  realTimeMs_ = 0;
  deltaMs_ = 0;
  stageDistanceMm_ = 0;
  segmentDistanceMm_ = 0;
  distanceWhileMittisPendingMm_ = 0;
  stageDistanceEnabled_ = pendingJatType_ == JatType::MANNED_JAT ||
                          pendingJatType_ == JatType::EMIT_JAT_OFFSET;
  state_ = CompetitionState::JAT_RESULT;
  const uint64_t duration =
      static_cast<uint64_t>(settings_.jatResultSeconds) * 1000ULL;
  jatResultUntilMonotonicMs_ =
      duration > std::numeric_limits<uint64_t>::max() - monotonicMs
          ? std::numeric_limits<uint64_t>::max()
          : monotonicMs + duration;
  if (duration == 0) beginNextStartTimeEdit();
  return PointResult::JAT_COMPLETED;
}

void CompetitionEngine::beginNextStartTimeEdit() {
  if (state_ != CompetitionState::JAT_RESULT) return;
  if (pendingJatType_ == JatType::EMIT_JAT_OFFSET) {
    proposedStartClockTime_ = clockFromSeconds(
        static_cast<int64_t>(secondsOfDay(arrivalClockTime_)) +
        static_cast<int64_t>(pendingJatOffsetMinutes_) * 60LL);
  } else {
    proposedStartClockTime_ = roundedMinuteProposal(arrivalClockTime_);
  }
  proposedStartClockTime_.second = 0;
  state_ = CompetitionState::EDIT_START_TIME;
}

void CompetitionEngine::dismissJatResult(uint64_t monotonicMs) {
  (void)monotonicMs;
  beginNextStartTimeEdit();
}

bool CompetitionEngine::acceptNextStageStart(
    EventClockTime acceptedClockTime, uint32_t nowMillisecondsOfDay,
    uint64_t monotonicMs) {
  if (state_ != CompetitionState::EDIT_START_TIME ||
      acceptedClockTime.hour > 23 || acceptedClockTime.minute > 59 ||
      acceptedClockTime.second != 0 || nowMillisecondsOfDay >= DAY_MS)
    return false;
  proposedStartClockTime_ = acceptedClockTime;
  const int64_t acceptedMs =
      static_cast<int64_t>(secondsOfDay(acceptedClockTime)) * 1000LL;
  int64_t untilStartMs = acceptedMs - nowMillisecondsOfDay;
  if (untilStartMs > HALF_DAY_MS) untilStartMs -= DAY_MS;
  if (untilStartMs < -HALF_DAY_MS) untilStartMs += DAY_MS;
  startTimelineMs_ = saturatingAddInt64(saturatingToInt64(monotonicMs),
                                        untilStartMs);
  if (!stageDistanceEnabled_) {
    stageDistanceMm_ = 0;
    segmentDistanceMm_ = 0;
    stageDistanceEnabled_ = true;
  }
  realTimeMs_ = -untilStartMs;
  idealBeforeSegmentMs_ = 0;
  deltaFrozen_ = false;
  acceptedStartTimelineMs_ = startTimelineMs_;
  acceptedStartClockTime_ = acceptedClockTime;
  correctingStartTime_ = false;
  state_ = untilStartMs > 0 ? CompetitionState::WAIT_START
                            : CompetitionState::RUNNING;
  updateCalculation(monotonicMs);
  return true;
}

bool CompetitionEngine::beginStartTimeCorrection() {
  if (state_ != CompetitionState::WAIT_START) return false;
  correctingStartTime_ = true;
  acceptedStartTimelineMs_ = startTimelineMs_;
  proposedStartClockTime_ = acceptedStartClockTime_;
  state_ = CompetitionState::EDIT_START_TIME;
  return true;
}

void CompetitionEngine::rejectStartTimeCorrection(uint64_t monotonicMs) {
  if (!correctingStartTime_ || state_ != CompetitionState::EDIT_START_TIME)
    return;
  startTimelineMs_ = acceptedStartTimelineMs_;
  correctingStartTime_ = false;
  state_ = CompetitionState::WAIT_START;
  updateCalculation(monotonicMs);
}

uint16_t CompetitionEngine::nextJatOrFinishPointIndex(
    uint16_t startSegmentIndex) const {
  if (!active_ || startSegmentIndex >= order_.segments.size()) return 0;
  for (size_t index = startSegmentIndex; index < order_.segments.size(); ++index) {
    const SegmentDefinition& segment = order_.segments[index];
    if (segment.pointTypeAtEnd == PointType::JAT ||
        segment.pointTypeAtEnd == PointType::FINISH_M)
      return segment.endPointIndex;
  }
  return 0;
}

bool CompetitionEngine::applyOverride(const SegmentOverride& requested,
                                      SegmentOverride& accepted) {
  if (state_ != CompetitionState::RUNNING ||
      requested.startSegmentIndex < currentSegmentIndex_ ||
      requested.startSegmentIndex >= order_.segments.size() ||
      requested.startSegmentIndex >=
          nextJatOrFinishPointIndex(currentSegmentIndex_) ||
      requested.endPointIndex <= requested.startSegmentIndex ||
      requested.endPointIndex > order_.segments.size() ||
      requested.endPointIndex >
          nextJatOrFinishPointIndex(requested.startSegmentIndex) ||
      requested.replacementDurationSeconds < 1 ||
      requested.replacementDurationSeconds > 3599)
    return false;
  if (requested.overrideType == OverrideType::ROAD_BREAK &&
      requested.replacementDurationSeconds != 660 &&
      requested.replacementDurationSeconds != 1260 &&
      requested.replacementDurationSeconds != 1860)
    return false;
  if (requested.overrideType == OverrideType::ROAD_BREAK &&
      requested.startSegmentIndex != currentSegmentIndex_)
    return false;
  accepted = requested;
  accepted.originalDefinitions.clear();
  for (uint16_t index = accepted.startSegmentIndex;
       index < accepted.endPointIndex; ++index)
    accepted.originalDefinitions.push_back(order_.segments[index]);
  activeOverride_ = accepted;
  hasActiveOverride_ = true;
  if (currentSegmentIndex_ == activeOverride_.startSegmentIndex)
    overrideIdealBeforeMs_ = idealBeforeSegmentMs_;
  updateCalculation(lastMonotonicMs_);
  return true;
}

PointResult CompetitionEngine::resolveMittisProposal() {
  if (!mittisProposal_.pending || state_ != CompetitionState::RUNNING)
    return PointResult::IGNORED;
  mittisProposal_ = MittisProposal{};
  return completeCurrentPoint(lastMonotonicMs_);
}

bool CompetitionEngine::undoPoint(uint64_t monotonicMs) {
  if (state_ != CompetitionState::RUNNING || !hasUndoPrompt(monotonicMs))
    return false;
  currentSegmentIndex_ = undo_.segmentIndex;
  segmentDistanceMm_ = undo_.segmentDistanceMillimeters;
  idealBeforeSegmentMs_ = undo_.idealBeforeSegmentMs;
  undo_.valid = false;
  updateCalculation(monotonicMs);
  return true;
}

bool CompetitionEngine::hasUndoPrompt(uint64_t monotonicMs) const {
  return undo_.valid && monotonicMs <= undo_.deadlineMonotonicMs;
}

}  // namespace domain
