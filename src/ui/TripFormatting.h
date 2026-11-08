#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace ui {

inline void formatTrip(char* text, size_t size, int64_t distanceMm,
                       bool tenMeterPrecision = false) {
  const bool negative = distanceMm < 0;
  const uint64_t magnitude =
      negative ? static_cast<uint64_t>(-(distanceMm + 1)) + 1
               : static_cast<uint64_t>(distanceMm);
  uint64_t meters = magnitude / 1000ULL;
  if (meters < 10000ULL) {
    if (tenMeterPrecision) meters = meters / 10ULL * 10ULL;
    std::snprintf(text, size, "%s%llu.%03llu", negative ? "-" : "",
                  static_cast<unsigned long long>(meters / 1000ULL),
                  static_cast<unsigned long long>(meters % 1000ULL));
  } else {
    const uint64_t tenMeters = meters / 10ULL;
    std::snprintf(text, size, "%s%llu.%02llu", negative ? "-" : "",
                  static_cast<unsigned long long>(tenMeters / 100ULL),
                  static_cast<unsigned long long>(tenMeters % 100ULL));
  }
}

}  // namespace ui
