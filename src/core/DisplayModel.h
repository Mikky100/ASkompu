#pragma once

#include <cstdint>

#include "Clock.h"

namespace core {

enum class Screen : uint8_t {
  StartupTimeEntry,
  BasicView,
  Menu,
  TimeEdit,
  CalibrationEdit,
  Diagnostics,
};

enum class TimeField : uint8_t { Hour, Minute };

struct TripDisplayModel {
  uint64_t distanceMillimeters;
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
  bool buttonPressed[5];
  uint8_t lastButtonId;
  uint8_t lastButtonEventType;
  uint64_t totalPulseCount;
  uint64_t trip1PulseCount;
  uint64_t trip2PulseCount;
  uint64_t trip1DistanceMillimeters;
  uint64_t trip2DistanceMillimeters;
  uint32_t millimetersPerPulse;
  float speedKmh;
  uint32_t lastPulseAgeMilliseconds;
  bool speedZeroTimedOut;
  bool clockSet;
  ClockTime clock;
  uint64_t clockElapsedMilliseconds;
};

struct DisplayModel {
  Screen screen;
  ClockTime clock;
  float speedKmh;
  TripDisplayModel trip1;
  TripDisplayModel trip2;
  TimeEntryDisplayModel timeEntry;
  CalibrationDisplayModel calibration;
  MenuDisplayModel menu;
  DiagnosticsDisplayModel diagnostics;
};

}  // namespace core
