#pragma once

#include <cstdint>

namespace domain {

enum class TextColor : uint8_t { WHITE = 0, RED = 1, GREEN = 2 };

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
