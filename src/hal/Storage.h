#pragma once

#include <Preferences.h>

#include "domain/Settings.h"

namespace hal {

class Storage {
 public:
  void begin();
  bool load(domain::Settings& settings);
  void saveCoefficient(uint32_t value);
  void saveTheme(domain::Theme theme);
  void saveClock(uint8_t hh, uint8_t mm);

 private:
  Preferences prefs_;
};

}  // namespace hal
