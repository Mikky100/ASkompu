#pragma once

#include <cstdint>
#include "Esp32S3Ili9488Pins.h"
#ifdef ARDUINO
#include <Arduino.h>
#else
constexpr uint8_t LOW = 0;
constexpr uint8_t INPUT_PULLUP = 0x05;
constexpr int RISING = 0x01;
#endif

#define ASKOMPU_HAS_TRIP1_RESET_PIN 1
#define ASKOMPU_HAS_FOOT_RESET_PIN 1

namespace BoardConfig {

constexpr uint16_t DISPLAY_WIDTH = 480;
constexpr uint16_t DISPLAY_HEIGHT = 320;
constexpr uint8_t DISPLAY_ROTATION = 1;
constexpr bool USE_PSRAM_SPRITE = true;
constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 50;

constexpr uint8_t PIN_BUTTON_LEFT = Esp32S3Ili9488Pins::BUTTON_LEFT;
constexpr uint8_t PIN_BUTTON_UP = Esp32S3Ili9488Pins::BUTTON_UP;
constexpr uint8_t PIN_BUTTON_DOWN = Esp32S3Ili9488Pins::BUTTON_DOWN;
constexpr uint8_t PIN_BUTTON_RIGHT = Esp32S3Ili9488Pins::BUTTON_RIGHT;
constexpr uint8_t PIN_BUTTON_POINT = Esp32S3Ili9488Pins::BUTTON_POINT;
constexpr uint8_t PIN_BUTTON_AT = Esp32S3Ili9488Pins::BUTTON_AT;
constexpr uint8_t PIN_BUTTON_TRIP1_RESET =
    Esp32S3Ili9488Pins::BUTTON_TRIP1_RESET;
constexpr uint8_t PIN_BUTTON_TRIP2_RESET =
    Esp32S3Ili9488Pins::BUTTON_TRIP2_RESET;
constexpr uint8_t PIN_PULSE_INPUT = Esp32S3Ili9488Pins::PULSE_INPUT;
constexpr uint8_t PIN_REVERSE_INPUT = Esp32S3Ili9488Pins::REVERSE_INPUT;
constexpr uint8_t PIN_BUTTON_FOOT_RESET =
    Esp32S3Ili9488Pins::BUTTON_FOOT_RESET;
constexpr uint8_t PIN_EXTERNAL_TRIP_TX_RESERVED =
    Esp32S3Ili9488Pins::EXTERNAL_TRIP_TX_RESERVED;
constexpr uint8_t PIN_EXTERNAL_TRIP_RX_RESERVED =
    Esp32S3Ili9488Pins::EXTERNAL_TRIP_RX_RESERVED;

constexpr uint8_t BUTTON_PRESSED_LEVEL = LOW;
constexpr uint8_t REVERSE_ACTIVE_LEVEL = LOW;
constexpr uint8_t REVERSE_INPUT_MODE = INPUT_PULLUP;
constexpr uint8_t PULSE_INPUT_MODE = INPUT_PULLUP;
constexpr int PULSE_INTERRUPT_MODE = RISING;

#ifdef ARDUINO
static_assert(TX == Esp32S3Ili9488Pins::UART0_TX,
              "ESP32-S3 UART0 TX must remain on GPIO43");
static_assert(RX == Esp32S3Ili9488Pins::UART0_RX,
              "ESP32-S3 UART0 RX must remain on GPIO44");
#endif

}  // namespace BoardConfig
