#pragma once

#include "domain/Diagnostics.h"
#include "domain/PointsModel.h"
#include "domain/Settings.h"
#include "domain/TripModel.h"
#include "ui/Layout.h"

namespace ui {

class Renderer {
 public:
  explicit Renderer(Layout& layout) : layout_(layout) {}

  void drawMain(const domain::Settings& settings, const domain::TripModel& trip,
                const domain::PointsModel& points, uint32_t effectivePulses);
  void drawClockEditor(uint8_t hh, uint8_t mm, bool editMinutes);
  void drawCoefficientEditor(uint32_t coefficient);
  void drawThemeEditor(domain::Theme theme);
  void drawDebug(const domain::Diagnostics& diag, uint32_t coefficient);

 private:
  Layout& layout_;
};

}  // namespace ui
