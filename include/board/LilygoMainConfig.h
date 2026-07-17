#pragma once

#include <cstdint>
#ifdef ARDUINO
#include <Arduino.h>
#else
constexpr uint8_t LOW = 0;
constexpr uint8_t INPUT_PULLUP = 0x05;
constexpr int RISING = 0x01;
#endif

namespace BoardConfig {

constexpr uint16_t DISPLAY_WIDTH = 320;
constexpr uint16_t DISPLAY_HEIGHT = 170;
constexpr uint8_t DISPLAY_ROTATION = 1;
constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 100;
constexpr bool USE_PSRAM_SPRITE = false;

constexpr uint8_t PIN_BUTTON_LEFT = 1;
constexpr uint8_t PIN_BUTTON_UP = 2;
constexpr uint8_t PIN_BUTTON_DOWN = 3;
constexpr uint8_t PIN_BUTTON_RIGHT = 10;
constexpr uint8_t PIN_BUTTON_TRIP2_RESET = 11;
constexpr uint8_t PIN_BUTTON_AT = 12;
constexpr uint8_t PIN_REVERSE_INPUT = 13;
constexpr uint8_t PIN_BUTTON_POINT = 14;
constexpr uint8_t PIN_PULSE_INPUT = 16;

constexpr uint8_t BUTTON_PRESSED_LEVEL = LOW;
constexpr uint8_t REVERSE_ACTIVE_LEVEL = LOW;
constexpr uint8_t REVERSE_INPUT_MODE = INPUT_PULLUP;
constexpr uint8_t PULSE_INPUT_MODE = INPUT_PULLUP;
constexpr int PULSE_INTERRUPT_MODE = RISING;

}  // namespace BoardConfig
