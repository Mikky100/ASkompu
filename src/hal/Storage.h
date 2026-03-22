#pragma once

#include <Preferences.h>

#include "domain/Settings.h"

namespace hal {

class Storage {
 public:
  void begin();
  void load(domain::Settings& settings);
  void saveCoefficient(uint32_t value);
  void saveTheme(domain::Theme theme);

 private:
  Preferences prefs_;
};

}  // namespace hal
