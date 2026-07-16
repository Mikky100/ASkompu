#include "CompetitionRecords.h"

#include <limits>

namespace domain {
namespace {

uint64_t magnitudeOfNegative(int64_t value) {
  return static_cast<uint64_t>(-(value + 1)) + 1ULL;
}

uint64_t saturatingMultiply(uint64_t value, uint64_t factor) {
  if (factor != 0 && value > std::numeric_limits<uint64_t>::max() / factor)
    return std::numeric_limits<uint64_t>::max();
  return value * factor;
}

}  // namespace

CompetitionSettings validatedCompetitionSettings(
    const CompetitionSettings& settings) {
  CompetitionSettings result = settings;
  if (result.lateFactor == 0) result.lateFactor = 1;
  if (result.earlyFactor == 0) result.earlyFactor = 3;
  if (result.jatResultSeconds > 20) result.jatResultSeconds = 5;
  if (result.atDisplayDistanceM < 1 || result.atDisplayDistanceM > 20)
    result.atDisplayDistanceM = 10;
  return result;
}

uint64_t saturatingPointsAdd(uint64_t left, uint64_t right) {
  return right > std::numeric_limits<uint64_t>::max() - left
             ? std::numeric_limits<uint64_t>::max()
             : left + right;
}

StageResult scoreStage(uint16_t stageIndex, PointType endPointType,
                       EventClockTime endClockTime, int64_t finalDeltaMs,
                       uint16_t lateFactor, uint16_t earlyFactor) {
  StageResult result;
  result.stageIndex = stageIndex;
  result.endPointType = endPointType;
  result.endClockTime = endClockTime;
  result.finalDeltaSeconds = finalDeltaMs / 1000;
  if (result.finalDeltaSeconds < 0)
    result.lateSeconds = magnitudeOfNegative(result.finalDeltaSeconds);
  else
    result.earlySeconds = static_cast<uint64_t>(result.finalDeltaSeconds);
  result.points = saturatingPointsAdd(
      saturatingMultiply(result.lateSeconds, lateFactor),
      saturatingMultiply(result.earlySeconds, earlyFactor));
  return result;
}

uint64_t totalStagePoints(const std::vector<StageResult>& results) {
  uint64_t total = 0;
  for (const StageResult& result : results)
    total = saturatingPointsAdd(total, result.points);
  return total;
}

}  // namespace domain
