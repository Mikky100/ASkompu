#pragma once

#include <cstdint>

#include "Clock.h"
#include "domain/DisplaySetting.h"
#include "domain/CompetitionEngine.h"
#include "route/RouteOrderEditor.h"

namespace core {

enum class Screen : uint8_t {
  StartupTimeEntry,
  BasicView,
  Menu,
  TimeEdit,
  CalibrationEdit,
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
  bool jatNotImplemented;
  bool pointLongPressNotImplemented;
  bool atNotImplemented;
};

constexpr uint8_t MENU_VISIBLE_ROWS = 5;

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
  TripDisplayModel trip1;
  TripDisplayModel trip2;
  TimeEntryDisplayModel timeEntry;
  CalibrationDisplayModel calibration;
  OrderDisplayModel order;
  OrderAccessDisplayModel orderAccess;
  CompetitionDisplayModel competition;
  MenuDisplayModel menu;
  DiagnosticsDisplayModel diagnostics;
};

}  // namespace core
