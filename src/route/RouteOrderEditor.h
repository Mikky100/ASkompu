#pragma once

#include <cstdint>

#include "domain/RouteOrder.h"

namespace route {

enum class EditorKey : uint8_t { UP, DOWN, LEFT, RIGHT };
enum class EditorPhase : uint8_t {
  COMPETITION_TYPE,
  START_TIME,
  SEGMENT_TYPE,
  VALUE,
  CONTINUATION,
  JAT_TYPE,
  JAT_OFFSET,
  STAGE_SELECT,
  BROWSE,
  CANCEL_PROMPT,
  SAVE_PENDING,
};
enum class EditorResult : uint8_t { NONE, EXIT, SAVE_REQUESTED };

struct RouteOrderEditorView {
  EditorPhase phase;
  bool editingExisting;
  bool saveFailed;
  domain::CompetitionType competitionType;
  uint8_t startHour;
  uint8_t startMinute;
  uint8_t startTimeField;
  domain::SegmentType segmentType;
  uint32_t value;
  bool enteringMittisTime;
  uint8_t digitCursor;
  uint8_t continuation;
  bool mittisAvailable;
  domain::JatType jatType;
  int16_t jatOffsetMinutes;
  uint16_t selectedStage;
  uint16_t stageCount;
  uint16_t selectedSegment;
  uint16_t segmentCount;
};

class RouteOrderEditor {
 public:
  void beginCreate();
  void beginBrowse(const domain::RouteOrder& current);
  EditorResult handle(EditorKey key, bool longPress = false);
  void completeSave(bool succeeded);

  const domain::RouteOrder& draft() const { return draft_; }
  RouteOrderEditorView view() const;

 private:
  void beginNewSegment(domain::SegmentType type = domain::SegmentType::TIME);
  void beginEditSelected();
  bool acceptValue();
  EditorResult acceptContinuation();
  EditorResult finishWorkingSegment(domain::PointType pointType);
  void adjustValue(int8_t direction);
  uint8_t digitCount() const;
  bool canSelectMittis() const;
  uint16_t stageCount() const;
  uint16_t firstSegmentOfStage(uint16_t stageNumber) const;
  uint16_t lastSegmentOfStage(uint16_t stageNumber) const;
  void returnFromNewSegment();

  domain::RouteOrder draft_;
  domain::SegmentDefinition working_;
  domain::SegmentDefinition originalWorking_;
  EditorPhase phase_ = EditorPhase::COMPETITION_TYPE;
  EditorPhase phaseBeforeCancel_ = EditorPhase::COMPETITION_TYPE;
  bool editingExisting_ = false;
  bool editingSegment_ = false;
  bool enteringMittisTime_ = false;
  bool saveFailed_ = false;
  uint8_t digitCursor_ = 0;
  uint8_t startTimeField_ = 0;
  uint8_t continuation_ = 0;
  uint8_t jatSelection_ = 0;
  uint16_t selectedStage_ = 1;
  uint16_t selectedSegment_ = 0;
  uint8_t originalStartHour_ = 0;
  uint8_t originalStartMinute_ = 0;
  bool editingStartTime_ = false;
  bool newSegmentFromContinuation_ = false;
  bool newSegmentAfterJat_ = false;
};

}  // namespace route
