#pragma once

#include <Arduino.h>

namespace BoardConfig {

// HUOM: Nuolinäppäinten GPIO-kartoitus pitää kopioida ESP32testCount-prototyypistä.
// Tässä versiossa käytetään väliaikaisia placeholder-arvoja, koska lähderepoon ei
// saatu verkkoyhteyttä kehitysympäristöstä.
constexpr uint8_t PIN_BUTTON_UP = 2;     // TODO: korvaa prototyypin arvolla
constexpr uint8_t PIN_BUTTON_DOWN = 3;   // TODO: korvaa prototyypin arvolla
constexpr uint8_t PIN_BUTTON_LEFT = 1;   // TODO: korvaa prototyypin arvolla
constexpr uint8_t PIN_BUTTON_RIGHT = 10;  // TODO: korvaa prototyypin arvolla
constexpr bool BUTTONS_ACTIVE_LOW = true;

constexpr uint8_t PIN_TRIP_RESET = 14;
constexpr bool TRIP_RESET_ACTIVE_LOW = true;

constexpr uint8_t PIN_SPEED_INPUT = 16;
constexpr bool SPEED_INPUT_RISING_EDGE = true;

}  // namespace BoardConfig
