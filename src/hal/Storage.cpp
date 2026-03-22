#include "Storage.h"

namespace {
constexpr const char* kNs = "askompu";
constexpr const char* kCoeff = "coeff";
constexpr const char* kTheme = "theme";
}  // namespace

namespace hal {

void Storage::begin() { prefs_.begin(kNs, false); }

void Storage::load(domain::Settings& settings) {
  settings.coefficient = prefs_.getULong(kCoeff, 1000);
  settings.theme = static_cast<domain::Theme>(prefs_.getUChar(kTheme, 0));
  settings.clock.set(12, 0);
}

void Storage::saveCoefficient(uint32_t value) { prefs_.putULong(kCoeff, value); }

void Storage::saveTheme(domain::Theme theme) {
  prefs_.putUChar(kTheme, static_cast<uint8_t>(theme));
}

}  // namespace hal
