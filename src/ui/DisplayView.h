#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "core/DisplayModel.h"

namespace ui {

class DisplayView {
 public:
  DisplayView();
  void begin();
  void render(const core::DisplayModel& model);

 private:
  void showBasicView(const core::DisplayModel& model);
  void showTimeEntry(const core::TimeEntryDisplayModel& model);
  void showMenu(const core::MenuDisplayModel& model);
  void showCalibration(const core::CalibrationDisplayModel& model);
  void showDiagnostics(const core::DiagnosticsDisplayModel& model);

  TFT_eSPI display_;
  TFT_eSprite canvas_;
};

}  // namespace ui
