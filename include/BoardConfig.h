#pragma once

#include <Arduino.h>

namespace BoardConfig {

constexpr uint16_t DISPLAY_WIDTH = 320;
constexpr uint16_t DISPLAY_HEIGHT = 170;

constexpr uint8_t PIN_BUTTON_LEFT = 1;
constexpr uint8_t PIN_BUTTON_UP = 2;
constexpr uint8_t PIN_BUTTON_DOWN = 3;
constexpr uint8_t PIN_BUTTON_RIGHT = 10;
constexpr uint8_t PIN_BUTTON_TRIP_RESET = 14;

constexpr uint8_t BUTTON_PRESSED_LEVEL = LOW;

}  // namespace BoardConfig
