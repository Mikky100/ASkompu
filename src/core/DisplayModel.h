#pragma once

#include <cstdint>

namespace core {

enum class Screen : uint8_t {
  Drive,
  Calibration,
};

struct TripDisplayModel {
  uint64_t distanceMillimeters;
  bool visible;
};

struct CalibrationDisplayModel {
  uint32_t editedMillimetersPerPulse;
  bool saveFailed;
};

struct DisplayModel {
  Screen screen;
  float speedKmh;
  TripDisplayModel trip1;
  TripDisplayModel trip2;
  uint64_t totalPulseCount;
  uint32_t millimetersPerPulse;
  CalibrationDisplayModel calibration;
};

}  // namespace core
