#pragma once

#include <Arduino.h>

namespace domain {

enum class Screen : uint8_t {
  Main = 0,
  Menu,
  Display,
  Settings,
  SettingsClock,
  SettingsCoefficient,
  SettingsTheme,
  Debug
};

struct MenuState {
  Screen current = Screen::Main;
  uint8_t menuIndex = 0;
  uint8_t settingsIndex = 0;
};

}  // namespace domain
