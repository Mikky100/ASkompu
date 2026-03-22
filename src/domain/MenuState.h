#pragma once

#include <Arduino.h>

namespace domain {

enum class Screen : uint8_t {
  Main = 0,
  SettingsClock,
  SettingsCoefficient,
  SettingsTheme,
  Debug
};

struct MenuState {
  Screen current = Screen::SettingsClock;
};

}  // namespace domain
