#pragma once

#include <cstdint>
#include <vector>

#include "RouteOrder.h"

namespace domain {

struct EventClockTime {
  EventClockTime() = default;
  EventClockTime(uint8_t hourValue, uint8_t minuteValue, uint8_t secondValue)
      : hour(hourValue), minute(minuteValue), second(secondValue) {}
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

struct CompetitionSettings {
  uint16_t lateFactor = 1;
  uint16_t earlyFactor = 3;
  uint8_t jatResultSeconds = 5;
  uint8_t atDisplayDistanceM = 10;
};

CompetitionSettings validatedCompetitionSettings(
    const CompetitionSettings& settings);

enum class DomainEventType : uint8_t {
  NORMAL_POINT,
  POINT_UNDO,
  AT,
  AT_CANCELLED,
  JAT,
  FINISH,
  START_TIME_PROPOSED,
  START_TIME_ACCEPTED,
  START_TIME_CORRECTED,
  MITTIS_PROPOSED,
  MITTIS_ACCEPTED,
  MITTIS_REJECTED,
  ADDITIONAL_ORDER,
  ROAD_BREAK,
  REVERSE_CHANGED,
  TRIP_RESET,
};

enum class OverrideType : uint8_t { ADDITIONAL_ORDER, ROAD_BREAK };
enum class TripChannel : uint8_t { TRIP_1, TRIP_2 };

struct StageResult {
  uint16_t stageIndex = 0;
  PointType endPointType = PointType::JAT;
  EventClockTime endClockTime;
  int64_t finalDeltaSeconds = 0;
  uint64_t lateSeconds = 0;
  uint64_t earlySeconds = 0;
  uint64_t points = 0;
};

struct SegmentOverride {
  OverrideType overrideType = OverrideType::ADDITIONAL_ORDER;
  uint16_t startSegmentIndex = 0;
  uint16_t endPointIndex = 0;
  uint16_t replacementDurationSeconds = 0;
  std::vector<SegmentDefinition> originalDefinitions;
  EventClockTime acceptedClockTime;
};

struct EventPayload {
  uint64_t referencedEventId = 0;
  bool hasStageResult = false;
  StageResult stageResult;
  bool hasJatType = false;
  JatType jatType = JatType::MANNED_JAT;
  EventClockTime arrivalClockTime;
  int64_t finalDeltaMs = 0;
  EventClockTime proposedStartClockTime;
  EventClockTime acceptedStartClockTime;
  uint32_t oldCalibration = 0;
  uint32_t proposedCalibration = 0;
  TripChannel tripChannel = TripChannel::TRIP_1;
  bool reverseActive = false;
  bool hasOverride = false;
  SegmentOverride segmentOverride;
};

struct EventRecord {
  uint64_t eventId = 0;
  DomainEventType eventType = DomainEventType::NORMAL_POINT;
  EventClockTime clockTime;
  uint64_t monotonicTimeMs = 0;
  uint16_t stageIndex = 0;
  uint16_t segmentIndex = 0;
  int64_t competitionTimeMs = 0;
  int64_t tIdealMs = 0;
  int64_t deltaMs = 0;
  int64_t physicalDistanceMillimeters = 0;
  int64_t stageDistanceMillimeters = 0;
  int64_t segmentDistanceMillimeters = 0;
  int64_t trip1DistanceMillimeters = 0;
  int64_t trip2DistanceMillimeters = 0;
  int32_t speedKmhMilli = 0;
  bool reverseActive = false;
  EventPayload payload;
  bool cancelled = false;
};

StageResult scoreStage(uint16_t stageIndex, PointType endPointType,
                       EventClockTime endClockTime, int64_t finalDeltaMs,
                       uint16_t lateFactor, uint16_t earlyFactor);
uint64_t saturatingPointsAdd(uint64_t left, uint64_t right);
uint64_t totalStagePoints(const std::vector<StageResult>& results);

}  // namespace domain
