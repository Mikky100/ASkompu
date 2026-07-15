#include "RouteOrder.h"

#include <limits>

namespace domain {

bool isJatTypeAllowed(CompetitionType competitionType, JatType jatType) {
  if (competitionType == CompetitionType::NON_EMIT) {
    return jatType == JatType::MANNED_JAT;
  }
  return jatType == JatType::EMIT_JAT_OFFSET ||
         jatType == JatType::EMIT_MLA || jatType == JatType::EMIT_ULA;
}

RouteOrderValidationError validateRouteOrder(const RouteOrder& order) {
  if (order.schemaVersion != ROUTE_ORDER_SCHEMA_VERSION) {
    return RouteOrderValidationError::UNSUPPORTED_SCHEMA;
  }
  if (order.startHour > 23 || order.startMinute > 59) {
    return RouteOrderValidationError::INVALID_START_TIME;
  }
  if (order.segments.empty()) {
    return RouteOrderValidationError::EMPTY;
  }
  if (order.competitionType != CompetitionType::EMIT &&
      order.competitionType != CompetitionType::NON_EMIT) {
    return RouteOrderValidationError::INVALID_JAT;
  }

  bool finishFound = false;
  for (size_t index = 0; index < order.segments.size(); ++index) {
    const SegmentDefinition& segment = order.segments[index];
    if (segment.segmentType != SegmentType::TIME &&
        segment.segmentType != SegmentType::SPEED &&
        segment.segmentType != SegmentType::MITTIS) {
      return RouteOrderValidationError::INVALID_VALUE;
    }
    if (segment.segmentIndex != index || segment.startPointIndex != index ||
        segment.endPointIndex != index + 1) {
      return RouteOrderValidationError::INDEX_ORDER;
    }
    if (segment.pointTypeAtEnd != PointType::NORMAL &&
        segment.pointTypeAtEnd != PointType::JAT &&
        segment.pointTypeAtEnd != PointType::FINISH_M) {
      return RouteOrderValidationError::INVALID_POINT;
    }
    if (segment.value == 0 ||
        (segment.segmentType == SegmentType::TIME && segment.value > 3599) ||
        (segment.segmentType == SegmentType::SPEED && segment.value > 99) ||
        (segment.segmentType == SegmentType::MITTIS &&
         (segment.value < 1000 || segment.value > 9999))) {
      return RouteOrderValidationError::INVALID_VALUE;
    }

    if (segment.pointTypeAtEnd == PointType::JAT) {
      if (!segment.hasJatType ||
          !isJatTypeAllowed(order.competitionType, segment.jatType)) {
        return RouteOrderValidationError::INVALID_JAT;
      }
      const bool offsetRequired = segment.jatType == JatType::EMIT_JAT_OFFSET;
      const bool offsetForbidden = segment.jatType == JatType::EMIT_MLA ||
                                   segment.jatType == JatType::EMIT_ULA;
      if ((offsetRequired && !segment.hasJatOffsetMinutes) ||
          (offsetForbidden && segment.hasJatOffsetMinutes)) {
        return RouteOrderValidationError::INVALID_JAT;
      }
    } else if (segment.hasJatType || segment.hasJatOffsetMinutes) {
      return RouteOrderValidationError::INVALID_POINT;
    }
    if (segment.segmentType == SegmentType::MITTIS &&
        (!segment.hasMittisDuration || segment.mittisDurationSeconds == 0 ||
         segment.mittisDurationSeconds > 3599)) {
      return RouteOrderValidationError::INVALID_MITTIS;
    }
    if (segment.segmentType != SegmentType::MITTIS &&
        (segment.hasMittisDuration || segment.mittisDurationSeconds != 0)) {
      return RouteOrderValidationError::INVALID_MITTIS;
    }

    if (segment.pointTypeAtEnd == PointType::FINISH_M) {
      finishFound = true;
      if (index + 1 != order.segments.size()) {
        return RouteOrderValidationError::FINISH_NOT_LAST;
      }
    }
  }
  return finishFound ? RouteOrderValidationError::NONE
                     : RouteOrderValidationError::MISSING_FINISH;
}

}  // namespace domain
