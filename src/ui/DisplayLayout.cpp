#include "DisplayLayout.h"

#include <cstring>

namespace ui {

DisplayLayout layoutFor(uint16_t width, uint16_t height) {
  if (width == 480 && height == 320) {
    return {480, 320,
            {4, 4, 228, 48}, {240, 4, 236, 98}, {4, 54, 228, 48},
            {4, 108, 148, 176}, {156, 108, 168, 176},
            {328, 108, 148, 176}, {0, 288, 480, 32},
            {4, 2}, {3, 2}, {4, 2}, {3, 2}, {2, 2}};
  }
  return {320, 170,
          {2, 2, 150, 26}, {158, 2, 160, 54}, {2, 30, 150, 26},
          {2, 60, 96, 88}, {101, 60, 118, 88}, {224, 60, 94, 88},
          {0, 153, 320, 17},
          {2, 2}, {2, 2}, {2, 2}, {3, 2}, {1, 2}};
}

bool isInside(const WidgetRect& rect, uint16_t width, uint16_t height) {
  return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 &&
         rect.right() <= static_cast<int16_t>(width) &&
         rect.bottom() <= static_cast<int16_t>(height);
}

bool overlaps(const WidgetRect& a, const WidgetRect& b) {
  return a.x < b.right() && a.right() > b.x && a.y < b.bottom() &&
         a.bottom() > b.y;
}

bool validateLayout(const DisplayLayout& layout) {
  const WidgetRect rects[] = {layout.trip1, layout.clock, layout.trip2,
                              layout.currentSegment, layout.delta,
                              layout.nextSegment, layout.debugSpeed};
  for (const WidgetRect& rect : rects)
    if (!isInside(rect, layout.width, layout.height)) return false;
  return !overlaps(layout.trip1, layout.clock) &&
         !overlaps(layout.clock, layout.trip2) &&
         !overlaps(layout.trip1, layout.trip2) &&
         !overlaps(layout.currentSegment, layout.delta) &&
         !overlaps(layout.delta, layout.nextSegment) &&
         !overlaps(layout.currentSegment, layout.nextSegment) &&
         !overlaps(layout.debugSpeed, layout.currentSegment) &&
         !overlaps(layout.debugSpeed, layout.delta) &&
         !overlaps(layout.debugSpeed, layout.nextSegment);
}

uint16_t estimatedTextWidth(const char* text, FontScale scale) {
  uint16_t width = 0;
  for (const char* cursor = text; *cursor; ++cursor) {
    const char character = *cursor;
    uint8_t glyphWidth = 0;
    if (scale.face == 4) {
      if (character >= '0' && character <= '9')
        glyphWidth = 14;
      else if (character == ' ')
        glyphWidth = 5;
      else if (character == ':' || character == '.')
        glyphWidth = 7;
      else if (character == '-' || character == '/')
        glyphWidth = 8;
      else if (character == '+')
        glyphWidth = 10;
      else
        glyphWidth = 18;
    } else {
      if (character >= '0' && character <= '9')
        glyphWidth = 8;
      else if (character == ':' )
        glyphWidth = 3;
      else if (character == '.')
        glyphWidth = 5;
      else if (character == '-' || character == '+')
        glyphWidth = 6;
      else if (character == ' ')
        glyphWidth = 6;
      else
        glyphWidth = 10;
    }
    width = static_cast<uint16_t>(width + glyphWidth * scale.value);
  }
  return width;
}

}  // namespace ui
