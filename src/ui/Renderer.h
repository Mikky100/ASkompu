#pragma once

#include "domain/Diagnostics.h"
#include "domain/MenuState.h"
#include "domain/Settings.h"
#include "domain/TripModel.h"
#include "ui/Layout.h"

namespace ui {

class Renderer {
 public:
  explicit Renderer(Layout& layout) : layout_(layout) {}

  void drawMain(const domain::Settings& settings, const domain::TripModel& trip,
                uint32_t effectivePulses);
  void drawMenu(uint8_t selected);
  void drawSettingsMenu(uint8_t selected);
  void drawClockEditor(uint8_t hh, uint8_t mm, bool editMinutes);
  void drawCoefficientEditor(uint32_t coefficient);
  void drawThemeEditor(domain::Theme theme);
  void drawDebug(const domain::Diagnostics& diag, uint32_t coefficient);

 private:
  Layout& layout_;
  void clearWithTheme(domain::Theme theme);
};

}  // namespace ui
