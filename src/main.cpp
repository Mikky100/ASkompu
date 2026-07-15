#include <Arduino.h>

#include "BoardConfig.h"
#include "CalibrationConfig.h"
#include "DemoConfig.h"
#include "core/ApplicationCore.h"
#include "input/DebouncedButton.h"
#include "input/PulseInput.h"
#include "ports/ArduinoClock.h"
#include "settings/SettingsRepository.h"
#include "settings/PreferencesRouteOrderStore.h"
#include "ui/DisplayView.h"

namespace {

constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t POLL_INTERVAL_MS = 10;

input::DebouncedButton leftButton(BoardConfig::PIN_BUTTON_LEFT,
                                  BoardConfig::BUTTON_PRESSED_LEVEL,
                                  DEBOUNCE_MS);
input::DebouncedButton upButton(BoardConfig::PIN_BUTTON_UP,
                                BoardConfig::BUTTON_PRESSED_LEVEL,
                                DEBOUNCE_MS,
                                CalibrationConfig::LONG_PRESS_DELAY_MS,
                                CalibrationConfig::REPEAT_INTERVAL_MS);
input::DebouncedButton downButton(BoardConfig::PIN_BUTTON_DOWN,
                                  BoardConfig::BUTTON_PRESSED_LEVEL,
                                  DEBOUNCE_MS,
                                  CalibrationConfig::LONG_PRESS_DELAY_MS,
                                  CalibrationConfig::REPEAT_INTERVAL_MS);
input::DebouncedButton rightButton(BoardConfig::PIN_BUTTON_RIGHT,
                                   BoardConfig::BUTTON_PRESSED_LEVEL,
                                   DEBOUNCE_MS,
                                   CalibrationConfig::LONG_PRESS_DELAY_MS,
                                   0);
input::DebouncedButton tripResetButton(BoardConfig::PIN_BUTTON_TRIP_RESET,
                                       BoardConfig::BUTTON_PRESSED_LEVEL,
                                       DEBOUNCE_MS);
input::PulseInput pulseInput(BoardConfig::PIN_PULSE_INPUT,
                             BoardConfig::PULSE_INPUT_MODE,
                             BoardConfig::PULSE_INTERRUPT_MODE);

ports::ArduinoClock clockSource;
core::SoftwareClock softwareClock(clockSource);
core::ApplicationCore application(
    softwareClock, CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE,
    DemoConfig::zeroSpeedTimeoutUs);
settings::SettingsRepository settingsRepository;
settings::PreferencesRouteOrderStore routeOrderStore;
ui::DisplayView view;
uint32_t lastDisplayUpdateMs = 0;
core::Screen renderedScreen = core::Screen::StartupTimeEntry;

void dispatchButton(input::DebouncedButton& button, core::ButtonId id,
                    uint32_t nowMs) {
  if (button.consumePressedEvent()) {
    application.handleButton({id, core::ButtonEventType::Press, nowMs});
  }
  if (button.consumeReleasedEvent()) {
    application.handleButton({id, core::ButtonEventType::Release, nowMs});
  }
  if (button.consumeLongPressEvent()) {
    application.handleButton({id, core::ButtonEventType::LongStart, nowMs});
  }
  if (button.consumeRepeatEvent()) {
    application.handleButton({id, core::ButtonEventType::LongRepeat, nowMs});
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  const settings::CalibrationLoadResult calibration =
      settingsRepository.loadCalibration();
  application.setInitialMillimetersPerPulse(calibration.millimetersPerPulse);
  const settings::TextColorLoadResult textColor =
      settingsRepository.loadTextColor();
  application.setInitialTextColor(textColor.color);
  Serial.printf("Calibration: %lu mm/pulse%s\n",
                static_cast<unsigned long>(calibration.millimetersPerPulse),
                calibration.usedDefault ? " (default)" : " (NVS)");
  domain::RouteOrder routeOrder;
  if (routeOrderStore.load(routeOrder)) {
    application.setInitialRouteOrder(routeOrder);
    Serial.printf("Route order: %u segments (NVS)\n",
                  static_cast<unsigned>(routeOrder.segments.size()));
  }

  leftButton.begin();
  upButton.begin();
  downButton.begin();
  rightButton.begin();
  tripResetButton.begin();
  pulseInput.begin();

  if (tripResetButton.isPressed()) {
    application.handleButton(
        {core::ButtonId::Trip1Reset, core::ButtonEventType::Press,
         clockSource.monotonicMilliseconds()});
  }

  view.begin();
  view.render(application.displayModel());
}

void loop() {
  const uint32_t nowMs = clockSource.monotonicMilliseconds();

  leftButton.update(nowMs);
  upButton.update(nowMs);
  downButton.update(nowMs);
  rightButton.update(nowMs);
  tripResetButton.update(nowMs);

  const input::PulseSnapshot pulseSnapshot = pulseInput.consumeSnapshot();
  const uint32_t nowUs = clockSource.monotonicMicroseconds();

  dispatchButton(leftButton, core::ButtonId::Left, nowMs);
  dispatchButton(upButton, core::ButtonId::Up, nowMs);
  dispatchButton(downButton, core::ButtonId::Down, nowMs);
  dispatchButton(rightButton, core::ButtonId::Right, nowMs);
  dispatchButton(tripResetButton, core::ButtonId::Trip1Reset, nowMs);

  if (pulseSnapshot.pendingPulses > 0) {
    Serial.printf("GPIO16 pulses: %lu, total: %lu\n",
                  static_cast<unsigned long>(pulseSnapshot.pendingPulses),
                  static_cast<unsigned long>(pulseSnapshot.totalPulses));
  }

  application.handleDistancePulses(
      {pulseSnapshot.pendingPulses, pulseSnapshot.previousPulseAtUs,
       pulseSnapshot.lastPulseAtUs, nowUs});
  application.tick(nowUs);

  uint32_t calibrationToSave = 0;
  if (application.takeCalibrationSaveRequest(calibrationToSave)) {
    const bool saved = settingsRepository.saveCalibration(calibrationToSave);
    application.completeCalibrationSave(saved);
    if (saved) {
      Serial.printf("Saved calibration: %lu mm/pulse\n",
                    static_cast<unsigned long>(calibrationToSave));
    }
  }

  domain::TextColor textColorToSave = domain::TextColor::WHITE;
  if (application.takeTextColorSaveRequest(textColorToSave)) {
    application.completeTextColorSave(
        settingsRepository.saveTextColor(textColorToSave));
  }

  const domain::RouteOrder* orderToSave = nullptr;
  if (application.takeRouteOrderSaveRequest(orderToSave)) {
    const bool saved = orderToSave && routeOrderStore.replace(*orderToSave);
    application.completeRouteOrderSave(saved);
    Serial.printf("Route order save: %s\n", saved ? "ok" : "failed");
  }

  const core::DisplayModel displayModel = application.displayModel();
  const bool screenChanged = displayModel.screen != renderedScreen;
  if (screenChanged ||
      nowMs - lastDisplayUpdateMs >= DemoConfig::displayUpdateIntervalMs) {
    view.render(displayModel);
    renderedScreen = displayModel.screen;
    lastDisplayUpdateMs = nowMs;
  }
  delay(POLL_INTERVAL_MS);
}
