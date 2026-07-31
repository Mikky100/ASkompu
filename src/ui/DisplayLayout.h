#pragma once

#include <cstdint>

namespace ui {

struct WidgetRect {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;

  int16_t right() const { return x + width; }
  int16_t bottom() const { return y + height; }
};

struct FontScale {
  uint8_t value;
  uint8_t face;
};

struct DisplayLayout {
  uint16_t width;
  uint16_t height;
  WidgetRect trip1;
  WidgetRect clock;
  WidgetRect currentSegment;
  WidgetRect delta;
  WidgetRect nextSegment;
  WidgetRect debugSpeed;
  FontScale topFont;
  FontScale clockFont;
  FontScale segmentFont;
  FontScale segmentValueFont;
  FontScale deltaFont;
  FontScale debugFont;
};

DisplayLayout layoutFor(uint16_t width, uint16_t height);
bool isInside(const WidgetRect& rect, uint16_t width, uint16_t height);
bool overlaps(const WidgetRect& first, const WidgetRect& second);
bool validateLayout(const DisplayLayout& layout);
uint16_t estimatedTextWidth(const char* text, FontScale scale);

}  // namespace ui
