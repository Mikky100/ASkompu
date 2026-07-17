#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "core/DisplayModel.h"
#include "DisplayPort.h"

namespace ui {

class DisplayView : public DisplayPort {
 public:
  DisplayView();
  bool begin() override;
  void setBacklight(bool enabled) override;
  uint16_t width() const override;
  uint16_t height() const override;
  void render(const core::DisplayModel& model) override;

 private:
  void showBasicView(const core::DisplayModel& model);
  void showTimeEntry(const core::TimeEntryDisplayModel& model);
  void showMenu(const core::MenuDisplayModel& model);
  void showCalibration(const core::CalibrationDisplayModel& model);
  void showMittis(const core::MittisDisplayModel& model);
  void showJatResult(const core::JatResultDisplayModel& model);
  void showFinishResult(const core::FinishResultDisplayModel& model);
  void showStartTimeEdit(const core::StartTimeEditDisplayModel& model);
  void showOverrideMenu(const core::OverrideMenuDisplayModel& model);
  void showOverrideEdit(const core::OverrideEditDisplayModel& model);
  void showResultView(const core::ResultViewDisplayModel& model);
  void showOrderAccess(const core::OrderAccessDisplayModel& model);
  void showOrder(const core::OrderDisplayModel& model);
  void showDiagnostics(const core::DiagnosticsDisplayModel& model);

  TFT_eSPI display_;
  TFT_eSprite canvas_;
  uint16_t textColor_ = TFT_WHITE;
  bool spriteReady_ = false;
};

}  // namespace ui
