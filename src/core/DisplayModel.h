#pragma once

#include <cstdint>

#include "Clock.h"
#include "domain/DisplaySetting.h"
#include "domain/CompetitionEngine.h"
#include "domain/CompetitionRecords.h"
#include "route/RouteOrderEditor.h"

namespace core {

enum class Screen : uint8_t {
  StartupTimeEntry,
  BasicView,
  Menu,
  TimeEdit,
  CalibrationEdit,
  MittisProposal,
  JatResult,
  StartTimeEdit,
  OverrideMenu,
  OverrideEdit,
  ResultView,
  OrderAccessPrompt,
  OrderEdit,
  Diagnostics,
};

enum class TimeField : uint8_t { Hour, Minute };

struct TripDisplayModel {
  int64_t distanceMillimeters;
  bool visible;
};

struct TimeEntryDisplayModel {
  uint8_t hour;
  uint8_t minute;
  TimeField activeField;
  bool startup;
};

struct CalibrationDisplayModel {
  uint32_t editedMillimetersPerPulse;
  bool saveFailed;
};

struct MittisDisplayModel {
  uint32_t oldMillimetersPerPulse;
  uint32_t proposedMillimetersPerPulse;
  int64_t measuredDistanceMillimeters;
  uint32_t referenceDistanceMeters;
  bool valid;
  bool saveFailed;
};

struct AtOverlayDisplayModel {
  bool visible;
  domain::EventClockTime clockTime;
  bool waitingForStop;
  int64_t travelledDistanceMillimeters;
  uint8_t targetDistanceMeters;
};

struct JatResultDisplayModel {
  domain::EventClockTime arrivalClockTime;
  int64_t finalDeltaSeconds;
};

struct FinishResultDisplayModel {
  bool visible;
  domain::EventClockTime finishClockTime;
  uint64_t totalPoints;
};

struct StartTimeEditDisplayModel {
  domain::EventClockTime proposedClockTime;
  domain::JatType jatType;
  bool valueVisible;
  bool acceptsWithPoint;
};

enum class OverrideMenuAction : uint8_t { AdditionalOrder, RoadBreak };
enum class OverrideEditPhase : uint8_t { StartSegment, Duration, EndPoint };

struct OverrideMenuDisplayModel {
  OverrideMenuAction selectedAction;
};

struct OverrideEditDisplayModel {
  OverrideMenuAction action;
  OverrideEditPhase phase;
  uint16_t startSegmentIndex;
  uint16_t endPointIndex;
  uint16_t maximumEndPointIndex;
  uint16_t durationSeconds;
  bool invalid;
};

enum class ResultViewType : uint8_t { StageResults, TotalPoints, Events };

struct ResultViewDisplayModel {
  ResultViewType type;
  uint16_t selectedIndex;
  uint16_t itemCount;
  bool hasStageResult;
  domain::StageResult stageResult;
  uint64_t totalPoints;
  bool hasEvent;
  domain::EventRecord event;
};

struct OrderDisplayModel {
  route::RouteOrderEditorView editor;
  domain::SegmentDefinition selectedSegment;
  bool hasSelectedSegment;
  bool showsStartTime;
};

enum class OrderAccessAction : uint8_t { Edit, Replace };

struct OrderAccessDisplayModel {
  OrderAccessAction selectedAction;
};

struct CompetitionDisplayModel {
  domain::CompetitionState state;
  int64_t deltaSeconds;
  bool deltaFrozen;
  domain::SegmentDefinition currentSegment;
  bool hasCurrentSegment;
  domain::SegmentDefinition nextSegment;
  bool hasNextSegment;
  bool undoPromptVisible;
};

constexpr uint8_t MENU_VISIBLE_ROWS = 3;

struct MenuRowDisplayModel {
  const char* label;
  bool enabled;
};

struct MenuDisplayModel {
  const char* title;
  MenuRowDisplayModel rows[MENU_VISIBLE_ROWS];
  uint8_t visibleRowCount;
  uint8_t selectedVisibleRow;
  uint8_t selectedIndex;
  uint8_t totalRows;
  uint8_t scrollOffset;
};

struct DiagnosticsDisplayModel {
  Screen currentScreen;
  bool buttonPressed[8];
  uint8_t lastButtonId;
  uint8_t lastButtonEventType;
  uint64_t totalPulseCount;
  uint64_t trip1PulseCount;
  uint64_t trip2PulseCount;
  int64_t trip1DistanceMillimeters;
  int64_t trip2DistanceMillimeters;
  uint32_t millimetersPerPulse;
  float speedKmh;
  uint32_t lastPulseAgeMilliseconds;
  bool speedZeroTimedOut;
  bool clockSet;
  ClockTime clock;
  uint64_t clockElapsedMilliseconds;
  uint8_t lastPointEventType;
  uint8_t lastAtEventType;
  bool reverseActive;
};

struct DisplayModel {
  Screen screen;
  domain::TextColor textColor;
  ClockTime clock;
  float speedKmh;
  bool showSpeed;
  TripDisplayModel trip1;
  TripDisplayModel trip2;
  TimeEntryDisplayModel timeEntry;
  CalibrationDisplayModel calibration;
  MittisDisplayModel mittis;
  AtOverlayDisplayModel atOverlay;
  JatResultDisplayModel jatResult;
  FinishResultDisplayModel finishResult;
  StartTimeEditDisplayModel startTimeEdit;
  OverrideMenuDisplayModel overrideMenu;
  OverrideEditDisplayModel overrideEdit;
  ResultViewDisplayModel resultView;
  OrderDisplayModel order;
  OrderAccessDisplayModel orderAccess;
  CompetitionDisplayModel competition;
  MenuDisplayModel menu;
  DiagnosticsDisplayModel diagnostics;
};

}  // namespace core
