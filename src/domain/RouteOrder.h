#pragma once

#include <cstdint>
#include <vector>

namespace domain {

constexpr uint16_t ROUTE_ORDER_SCHEMA_VERSION = 3;

enum class CompetitionType : uint8_t { EMIT = 0, NON_EMIT = 1 };
enum class SegmentType : uint8_t { TIME = 0, SPEED = 1, MITTIS = 2 };
enum class PointType : uint8_t { NORMAL = 0, JAT = 1, FINISH_M = 2 };
enum class JatType : uint8_t {
  MANNED_JAT = 0,
  EMIT_JAT_OFFSET = 1,
  EMIT_MLA = 2,
  EMIT_ULA = 3,
};

struct SegmentDefinition {
  uint16_t segmentIndex = 0;
  uint16_t startPointIndex = 0;
  uint16_t endPointIndex = 1;
  SegmentType segmentType = SegmentType::TIME;
  uint32_t value = 0;
  bool hasMittisDuration = false;
  uint16_t mittisDurationSeconds = 0;
  PointType pointTypeAtEnd = PointType::NORMAL;
  bool hasJatType = false;
  JatType jatType = JatType::MANNED_JAT;
  bool hasJatOffsetMinutes = false;
  int16_t jatOffsetMinutes = 0;
};

struct RouteOrder {
  uint16_t schemaVersion = ROUTE_ORDER_SCHEMA_VERSION;
  CompetitionType competitionType = CompetitionType::EMIT;
  uint8_t startHour = 0;
  uint8_t startMinute = 0;
  std::vector<SegmentDefinition> segments;
};

enum class RouteOrderValidationError : uint8_t {
  NONE,
  UNSUPPORTED_SCHEMA,
  INVALID_START_TIME,
  EMPTY,
  INDEX_ORDER,
  INVALID_VALUE,
  INVALID_MITTIS,
  INVALID_POINT,
  INVALID_JAT,
  MISSING_FINISH,
  FINISH_NOT_LAST,
};

RouteOrderValidationError validateRouteOrder(const RouteOrder& order);
bool isJatTypeAllowed(CompetitionType competitionType, JatType jatType);

}  // namespace domain
