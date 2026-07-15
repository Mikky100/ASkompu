#pragma once

#include <cstdint>

namespace domain {

enum class TextColor : uint8_t { WHITE = 0, RED = 1, GREEN = 2 };

inline bool isValidTextColor(TextColor color) {
  return color == TextColor::WHITE || color == TextColor::RED ||
         color == TextColor::GREEN;
}

}  // namespace domain
