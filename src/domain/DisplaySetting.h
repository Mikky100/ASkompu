#pragma once

#include <cstdint>

namespace domain {

enum class TextColor : uint8_t { WHITE = 0, RED = 1, GREEN = 2 };

constexpr uint8_t DEFAULT_BACKLIGHT_PERCENT = 60;
constexpr uint8_t MIN_BACKLIGHT_PERCENT = 10;
constexpr uint8_t MAX_BACKLIGHT_PERCENT = 100;
constexpr uint8_t BACKLIGHT_STEP_PERCENT = 10;

struct DisplaySettings {
  DisplaySettings(uint8_t backlight = DEFAULT_BACKLIGHT_PERCENT,
                  bool labels = true)
      : backlightPercent(backlight), showLabels(labels) {}
  uint8_t backlightPercent;
  bool showLabels;
};

inline bool isValidDisplaySettings(const DisplaySettings& settings) {
  return settings.backlightPercent >= MIN_BACKLIGHT_PERCENT &&
         settings.backlightPercent <= MAX_BACKLIGHT_PERCENT &&
         settings.backlightPercent % BACKLIGHT_STEP_PERCENT == 0;
}

inline DisplaySettings validatedDisplaySettings(DisplaySettings settings) {
  if (!isValidDisplaySettings(settings))
    settings.backlightPercent = DEFAULT_BACKLIGHT_PERCENT;
  return settings;
}

enum class DebugDisplayElement : uint16_t { SPEED = 1U << 0 };

constexpr uint16_t DEBUG_DISPLAY_KNOWN_MASK =
    static_cast<uint16_t>(DebugDisplayElement::SPEED);

struct DebugDisplaySettings {
  explicit DebugDisplaySettings(uint16_t elements = 0)
      : enabledElements(elements) {}

  uint16_t enabledElements;

  bool enabled(DebugDisplayElement element) const {
    return (enabledElements & static_cast<uint16_t>(element)) != 0;
  }
};

inline DebugDisplaySettings validatedDebugDisplaySettings(
    DebugDisplaySettings settings) {
  settings.enabledElements &= DEBUG_DISPLAY_KNOWN_MASK;
  return settings;
}

inline bool isValidTextColor(TextColor color) {
  return color == TextColor::WHITE || color == TextColor::RED ||
         color == TextColor::GREEN;
}

}  // namespace domain
