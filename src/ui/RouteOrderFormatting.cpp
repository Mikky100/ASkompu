#include "RouteOrderFormatting.h"

#include <cstdio>

namespace ui {

const char* routeOrderEditorTitle(const route::RouteOrderEditorView& editor) {
  if (editor.phase == route::EditorPhase::START_TIME) return "LAHTOAIKA";
  if (editor.phase == route::EditorPhase::COMPETITION_TYPE)
    return "KILPAILUTYYPPI";
  return "AJOMAARAYS";
}

const char* competitionTypeLabel(domain::CompetitionType competitionType) {
  return competitionType == domain::CompetitionType::EMIT ? "EMIT" : "EI EMIT";
}

void formatDriveSegmentRange(char* text, std::size_t size,
                             const domain::SegmentDefinition& segment) {
  if (!text || size == 0) return;
  char start[8];
  char end[8];
  if (segment.startPointIndex == 0)
    std::snprintf(start, sizeof(start), "L");
  else
    std::snprintf(start, sizeof(start), "%u", segment.startPointIndex);
  if (segment.pointTypeAtEnd == domain::PointType::FINISH_M)
    std::snprintf(end, sizeof(end), "M");
  else
    std::snprintf(end, sizeof(end), "%u", segment.endPointIndex);
  std::snprintf(text, size, "%s-%s", start, end);
}

const char* driveSegmentLabel(const domain::SegmentDefinition& segment) {
  if (segment.pointTypeAtEnd == domain::PointType::FINISH_M) return "MAALI";
  if (segment.pointTypeAtEnd == domain::PointType::JAT) return "JAT";
  if (segment.segmentType == domain::SegmentType::MITTIS) return "MITTIS";
  return "";
}

void formatEditableRouteOrderValue(
    char* text, std::size_t size,
    const route::RouteOrderEditorView& editor) {
  if (!text || size == 0) return;
  char digits[12]{};
  uint8_t count = 0;
  const bool timeValue =
      editor.segmentType == domain::SegmentType::TIME ||
      editor.enteringMittisTime;
  if (timeValue) {
    std::snprintf(digits, sizeof(digits), "%02lu%02lu",
                  static_cast<unsigned long>(editor.value / 60),
                  static_cast<unsigned long>(editor.value % 60));
    count = 4;
  } else if (editor.segmentType == domain::SegmentType::SPEED) {
    std::snprintf(digits, sizeof(digits), "%02lu",
                  static_cast<unsigned long>(editor.value));
    count = 2;
  } else {
    std::snprintf(digits, sizeof(digits), "%04lu",
                  static_cast<unsigned long>(editor.value));
    count = 4;
  }
  std::size_t output = 0;
  for (uint8_t index = 0; index < count && output + 4 < size; ++index) {
    if (index == editor.digitCursor) text[output++] = '[';
    text[output++] = digits[index];
    if (index == editor.digitCursor) text[output++] = ']';
    if (timeValue && index == 1) text[output++] = ':';
  }
  text[output] = '\0';
}

}  // namespace ui
