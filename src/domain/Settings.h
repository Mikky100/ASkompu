#pragma once

#include "TimeModel.h"
#include "Theme.h"

namespace domain {

struct Settings {
  TimeModel clock;
  uint32_t coefficient = 1000;
  Theme theme = Theme::Red;
};

}  // namespace domain
