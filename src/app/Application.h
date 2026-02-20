#pragma once

#include "domain/Diagnostics.h"
#include "domain/MenuState.h"
#include "domain/Settings.h"
#include "domain/TripModel.h"
#include "hal/Buttons.h"
#include "hal/Display.h"
#include "hal/SpeedInput.h"
#include "hal/Storage.h"
#include "ui/Layout.h"
#include "ui/Renderer.h"

namespace app {

class Application {
 public:
  void setup();
  void loop();

 private:
  void handleEvent(const hal::ButtonEvent& event);
  void updateDiagnostics(unsigned long now);
  void render();

  hal::Display display_;
  hal::Buttons buttons_;
  hal::SpeedInput speedInput_;
  hal::Storage storage_;

  domain::Settings settings_;
  domain::TripModel trip_;
  domain::MenuState menu_;
  domain::Diagnostics diag_;

  ui::Layout* layout_ = nullptr;
  ui::Renderer* renderer_ = nullptr;

  unsigned long lastDiagMs_ = 0;
  unsigned long lastLoopHzMs_ = 0;
  uint16_t loopCounter_ = 0;

  uint8_t clockEditHours_ = 12;
  uint8_t clockEditMinutes_ = 0;
  bool editMinutes_ = false;
};

}  // namespace app
