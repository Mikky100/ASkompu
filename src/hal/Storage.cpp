#include "Storage.h"

namespace {
constexpr const char* kNs = "askompu";
constexpr const char* kCoeff = "coeff";
constexpr const char* kTheme = "theme";
constexpr const char* kHour = "hour";
constexpr const char* kMinute = "minute";
}  // namespace

namespace hal {

void Storage::begin() { prefs_.begin(kNs, false); }

void Storage::load(domain::Settings& settings) {
  settings.coefficient = prefs_.getULong(kCoeff, 1000);
  settings.theme = static_cast<domain::Theme>(prefs_.getUChar(kTheme, 0));
  uint8_t hh = prefs_.getUChar(kHour, 12);
  uint8_t mm = prefs_.getUChar(kMinute, 0);
  settings.clock.set(hh, mm);
}

void Storage::saveCoefficient(uint32_t value) { prefs_.putULong(kCoeff, value); }

void Storage::saveTheme(domain::Theme theme) {
  prefs_.putUChar(kTheme, static_cast<uint8_t>(theme));
}

void Storage::saveClock(uint8_t hh, uint8_t mm) {
  prefs_.putUChar(kHour, hh % 24);
  prefs_.putUChar(kMinute, mm % 60);
}

}  // namespace hal
