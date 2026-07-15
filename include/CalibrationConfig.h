#pragma once

#include <cstdint>

namespace CalibrationConfig {

constexpr uint32_t DEFAULT_MILLIMETERS_PER_PULSE = 1000;
constexpr uint32_t MIN_MILLIMETERS_PER_PULSE = 1;
constexpr uint32_t MAX_MILLIMETERS_PER_PULSE = 100000;
constexpr uint32_t MILLIMETERS_PER_PULSE_STEP = 1;

constexpr uint16_t SETTINGS_SCHEMA_VERSION = 2;

constexpr uint32_t LONG_PRESS_DELAY_MS = 500;
constexpr uint32_t REPEAT_INTERVAL_MS = 100;

}  // namespace CalibrationConfig
