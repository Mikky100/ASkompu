#include "CompetitionEngine.h"

#include <limits>

namespace domain {
namespace {

constexpr int64_t DAY_MS = 24LL * 60LL * 60LL * 1000LL;
constexpr int64_t HALF_DAY_MS = DAY_MS / 2;

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
  lastMonotonicMs_ = monotonicMs;
  const int64_t startOfDayMs =
      (static_cast<int64_t>(order.startHour) * 60 + order.startMinute) * 60000;
  int64_t untilStartMs = startOfDayMs - nowMillisecondsOfDay;
  if (untilStartMs > HALF_DAY_MS) untilStartMs -= DAY_MS;
  if (untilStartMs < -HALF_DAY_MS) untilStartMs += DAY_MS;
  startTimelineMs_ = saturatingAddInt64(saturatingToInt64(monotonicMs),
                                        untilStartMs);
  realTimeMs_ = -untilStartMs;
  idealBeforeSegmentMs_ = 0;
  physicalDistanceMm_ = stageDistanceMm_ = segmentDistanceMm_ = 0;
  reverseActive_ = false;
  deltaFrozen_ = false;
  jatNotImplemented_ = false;
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
  if (segment->segmentType == SegmentType::TIME)
    return static_cast<int64_t>(segment->value) * 1000LL;
  if (segment->segmentType == SegmentType::SPEED)
    return speedIdealMilliseconds(segmentDistanceMm_, segment->value);
  return 0;
}

void CompetitionEngine::updateCalculation(uint64_t monotonicMs) {
  if (!active_ || state_ == CompetitionState::FINISHED) return;
  lastMonotonicMs_ = monotonicMs;
  realTimeMs_ = saturatingAddInt64(saturatingToInt64(monotonicMs),
                                   -startTimelineMs_);
  if (state_ == CompetitionState::WAIT_START && realTimeMs_ >= 0)
    state_ = CompetitionState::RUNNING;
  idealTimeMs_ = saturatingAddInt64(idealBeforeSegmentMs_,
                                    activeSegmentIdealMs());
  deltaMs_ = saturatingAddInt64(idealTimeMs_, -realTimeMs_);
}

void CompetitionEngine::tick(uint64_t monotonicMs) {
  updateCalculation(monotonicMs);
  if (undo_.valid && monotonicMs > undo_.deadlineMonotonicMs) undo_.valid = false;
}

void CompetitionEngine::addDistanceMillimeters(int64_t deltaMillimeters) {
  if (!active_ || state_ == CompetitionState::FINISHED) return;
  physicalDistanceMm_ = saturatingAddInt64(physicalDistanceMm_, deltaMillimeters);
  stageDistanceMm_ = saturatingAddInt64(stageDistanceMm_, deltaMillimeters);
  segmentDistanceMm_ = saturatingAddInt64(segmentDistanceMm_, deltaMillimeters);
  updateCalculation(lastMonotonicMs_);
}

PointResult CompetitionEngine::pointReleased(uint64_t monotonicMs) {
  if (state_ != CompetitionState::RUNNING) return PointResult::IGNORED;
  updateCalculation(monotonicMs);
  const SegmentDefinition* segment = currentSegment();
  if (!segment) return PointResult::IGNORED;
  if (segment->pointTypeAtEnd == PointType::JAT) {
    jatNotImplemented_ = true;
    return PointResult::JAT_NOT_IMPLEMENTED;
  }
  if (segment->pointTypeAtEnd == PointType::FINISH_M) {
    deltaFrozen_ = true;
    state_ = CompetitionState::FINISHED;
    undo_.valid = false;
    return PointResult::FINISHED;
  }
  if (currentSegmentIndex_ + 1 >= order_.segments.size())
    return PointResult::IGNORED;
  undo_.segmentIndex = currentSegmentIndex_;
  undo_.segmentDistanceMillimeters = segmentDistanceMm_;
  undo_.idealBeforeSegmentMs = idealBeforeSegmentMs_;
  undo_.deadlineMonotonicMs = monotonicMs + POINT_UNDO_WINDOW_MS;
  undo_.valid = true;
  idealBeforeSegmentMs_ = idealTimeMs_;
  ++currentSegmentIndex_;
  segmentDistanceMm_ = 0;
  jatNotImplemented_ = false;
  updateCalculation(monotonicMs);
  return PointResult::ADVANCED;
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
