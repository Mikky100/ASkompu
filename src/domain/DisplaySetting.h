#pragma once

#include <cstdint>

namespace domain {

enum class TextColor : uint8_t { WHITE = 0, RED = 1, GREEN = 2 };
enum class MenuFontSize : uint8_t { SMALL = 0, MEDIUM = 1, LARGE = 2 };
enum class TripDisplayMode : uint8_t { TRIP_1 = 0, TRIP_2 = 1, BOTH = 2 };
enum class TripResetTarget : uint8_t { TRIP_1 = 0, TRIP_2 = 1 };

constexpr uint8_t DEFAULT_BACKLIGHT_PERCENT = 60;
constexpr uint8_t MIN_BACKLIGHT_PERCENT = 10;
constexpr uint8_t MAX_BACKLIGHT_PERCENT = 100;
constexpr uint8_t BACKLIGHT_STEP_PERCENT = 10;

struct DisplaySettings {
  DisplaySettings(uint8_t backlight = DEFAULT_BACKLIGHT_PERCENT,
                  bool labels = true,
                  MenuFontSize fontSize = MenuFontSize::MEDIUM,
                  TripDisplayMode tripMode = TripDisplayMode::TRIP_1,
                  TripResetTarget externalReset = TripResetTarget::TRIP_1,
                  TripResetTarget internalReset = TripResetTarget::TRIP_1)
      : backlightPercent(backlight),
        showLabels(labels),
        menuFontSize(fontSize),
        tripDisplayMode(tripMode),
        externalResetTarget(externalReset),
        internalResetTarget(internalReset) {}
  uint8_t backlightPercent;
  bool showLabels;
  MenuFontSize menuFontSize;
  TripDisplayMode tripDisplayMode;
  TripResetTarget externalResetTarget;
  TripResetTarget internalResetTarget;
};

inline bool isValidMenuFontSize(MenuFontSize value) {
  return value == MenuFontSize::SMALL || value == MenuFontSize::MEDIUM ||
         value == MenuFontSize::LARGE;
}

inline bool isValidTripDisplayMode(TripDisplayMode value) {
  return value == TripDisplayMode::TRIP_1 ||
         value == TripDisplayMode::TRIP_2 || value == TripDisplayMode::BOTH;
}

inline bool isValidTripResetTarget(TripResetTarget value) {
  return value == TripResetTarget::TRIP_1 ||
         value == TripResetTarget::TRIP_2;
}

inline bool isValidDisplaySettings(const DisplaySettings& settings) {
  return settings.backlightPercent >= MIN_BACKLIGHT_PERCENT &&
         settings.backlightPercent <= MAX_BACKLIGHT_PERCENT &&
         settings.backlightPercent % BACKLIGHT_STEP_PERCENT == 0 &&
         isValidMenuFontSize(settings.menuFontSize) &&
         isValidTripDisplayMode(settings.tripDisplayMode) &&
         isValidTripResetTarget(settings.externalResetTarget) &&
         isValidTripResetTarget(settings.internalResetTarget);
}

inline DisplaySettings validatedDisplaySettings(DisplaySettings settings) {
  if (settings.backlightPercent < MIN_BACKLIGHT_PERCENT ||
      settings.backlightPercent > MAX_BACKLIGHT_PERCENT ||
      settings.backlightPercent % BACKLIGHT_STEP_PERCENT != 0)
    settings.backlightPercent = DEFAULT_BACKLIGHT_PERCENT;
  if (!isValidMenuFontSize(settings.menuFontSize))
    settings.menuFontSize = MenuFontSize::MEDIUM;
  if (!isValidTripDisplayMode(settings.tripDisplayMode))
    settings.tripDisplayMode = TripDisplayMode::TRIP_1;
  if (!isValidTripResetTarget(settings.externalResetTarget))
    settings.externalResetTarget = TripResetTarget::TRIP_1;
  if (!isValidTripResetTarget(settings.internalResetTarget))
    settings.internalResetTarget = TripResetTarget::TRIP_1;
  return settings;
}

inline bool sameDisplaySettings(const DisplaySettings& first,
                                const DisplaySettings& second) {
  return first.backlightPercent == second.backlightPercent &&
         first.showLabels == second.showLabels &&
         first.menuFontSize == second.menuFontSize &&
         first.tripDisplayMode == second.tripDisplayMode &&
         first.externalResetTarget == second.externalResetTarget &&
         first.internalResetTarget == second.internalResetTarget;
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
