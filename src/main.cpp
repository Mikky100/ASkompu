#include <Arduino.h>

#include "BoardConfig.h"
#include "CalibrationConfig.h"
#include "DemoConfig.h"
#include "core/ApplicationCore.h"
#include "input/DebouncedButton.h"
#include "input/PulseInput.h"
#include "input/StableSignalFilter.h"
#include "ports/ArduinoClock.h"
#include "settings/SettingsRepository.h"
#include "settings/PreferencesRouteOrderStore.h"
#include "ui/DisplayView.h"

namespace {

constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t POLL_INTERVAL_MS = 10;
constexpr uint32_t POINT_LONG_PRESS_MS = 1200;
constexpr uint32_t REVERSE_STABILITY_MS = 20;

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
input::DebouncedButton pointButton(BoardConfig::PIN_BUTTON_POINT,
                                   BoardConfig::BUTTON_PRESSED_LEVEL,
                                   DEBOUNCE_MS, POINT_LONG_PRESS_MS, 0);
input::DebouncedButton atButton(BoardConfig::PIN_BUTTON_AT,
                                BoardConfig::BUTTON_PRESSED_LEVEL,
                                DEBOUNCE_MS);
input::DebouncedButton trip2ResetButton(BoardConfig::PIN_BUTTON_TRIP2_RESET,
                                        BoardConfig::BUTTON_PRESSED_LEVEL,
                                        DEBOUNCE_MS);
#ifdef ASKOMPU_HAS_TRIP1_RESET_PIN
input::DebouncedButton trip1ResetButton(BoardConfig::PIN_BUTTON_TRIP1_RESET,
                                        BoardConfig::BUTTON_PRESSED_LEVEL,
                                        DEBOUNCE_MS);
#endif
#ifdef ASKOMPU_HAS_FOOT_RESET_PIN
input::DebouncedButton footResetButton(BoardConfig::PIN_BUTTON_FOOT_RESET,
                                       BoardConfig::BUTTON_PRESSED_LEVEL,
                                       DEBOUNCE_MS);
#endif
input::StableSignalFilter reverseFilter(REVERSE_STABILITY_MS);
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

void dispatchPointButton(uint32_t nowMs) {
  if (pointButton.consumePressedEvent())
    application.handleButton(
        {core::ButtonId::Point, core::ButtonEventType::Press, nowMs});
  if (pointButton.consumeLongPressEvent())
    application.handleButton(
        {core::ButtonId::Point, core::ButtonEventType::LongStart, nowMs});
  if (pointButton.consumeReleasedEvent()) {
    const bool shortPress = pointButton.consumeShortPressEvent();
    if (shortPress)
      application.handleButton(
          {core::ButtonId::Point, core::ButtonEventType::Release, nowMs});
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
  application.setCompetitionSettings(
      settingsRepository.loadCompetitionSettings().settings);
  application.setInitialDebugDisplaySettings(
      settingsRepository.loadDebugDisplaySettings().settings);
  Serial.printf("Calibration: %lu mm/pulse%s\n",
                static_cast<unsigned long>(calibration.millimetersPerPulse),
                calibration.usedDefault ? " (default)" : " (NVS)");
  domain::RouteOrder routeOrder;
  if (routeOrderStore.load(routeOrder)) {
    application.setInitialRouteOrder(
        routeOrder,
        routeOrderStore.activity() == route::RouteOrderActivity::ACTIVE);
    Serial.printf("Route order: %u segments (NVS)\n",
                  static_cast<unsigned>(routeOrder.segments.size()));
  }

  leftButton.begin();
  upButton.begin();
  downButton.begin();
  rightButton.begin();
  pointButton.begin();
  atButton.begin();
  trip2ResetButton.begin();
#ifdef ASKOMPU_HAS_TRIP1_RESET_PIN
  trip1ResetButton.begin();
#endif
#ifdef ASKOMPU_HAS_FOOT_RESET_PIN
  footResetButton.begin();
#endif
  pinMode(BoardConfig::PIN_REVERSE_INPUT, BoardConfig::REVERSE_INPUT_MODE);
  const uint32_t inputNowMs = clockSource.monotonicMilliseconds();
  reverseFilter.reset(
      digitalRead(BoardConfig::PIN_REVERSE_INPUT) ==
          BoardConfig::REVERSE_ACTIVE_LEVEL,
      inputNowMs);
  application.handleReverseSignal({reverseFilter.active(), inputNowMs});
  pulseInput.begin();

  const bool displayReady = view.begin();
#if defined(TFT_BL) && defined(TFT_BACKLIGHT_ON)
  Serial.printf("Display: buffer=%s, backlight GPIO%u=%s (read=%u)\n",
                displayReady ? "ok" : "failed",
                static_cast<unsigned>(TFT_BL),
                TFT_BACKLIGHT_ON == HIGH ? "active HIGH" : "active LOW",
                static_cast<unsigned>(digitalRead(TFT_BL)));
#else
  Serial.printf("Display: buffer=%s, no backlight GPIO\n",
                displayReady ? "ok" : "failed");
#endif
  view.render(application.displayModel());
}

void loop() {
  const uint32_t nowMs = clockSource.monotonicMilliseconds();

  leftButton.update(nowMs);
  upButton.update(nowMs);
  downButton.update(nowMs);
  rightButton.update(nowMs);
  pointButton.update(nowMs);
  atButton.update(nowMs);
  trip2ResetButton.update(nowMs);
#ifdef ASKOMPU_HAS_TRIP1_RESET_PIN
  trip1ResetButton.update(nowMs);
#endif
#ifdef ASKOMPU_HAS_FOOT_RESET_PIN
  footResetButton.update(nowMs);
#endif

  const bool rawReverse =
      digitalRead(BoardConfig::PIN_REVERSE_INPUT) ==
      BoardConfig::REVERSE_ACTIVE_LEVEL;
  if (reverseFilter.update(rawReverse, nowMs))
    application.handleReverseSignal({reverseFilter.active(), nowMs});

  const input::PulseSnapshot pulseSnapshot = pulseInput.consumeSnapshot();
  const uint32_t nowUs = clockSource.monotonicMicroseconds();

  dispatchButton(leftButton, core::ButtonId::Left, nowMs);
  dispatchButton(upButton, core::ButtonId::Up, nowMs);
  dispatchButton(downButton, core::ButtonId::Down, nowMs);
  dispatchButton(rightButton, core::ButtonId::Right, nowMs);
  dispatchPointButton(nowMs);
  dispatchButton(atButton, core::ButtonId::At, nowMs);
  dispatchButton(trip2ResetButton, core::ButtonId::Trip2Reset, nowMs);
#ifdef ASKOMPU_HAS_TRIP1_RESET_PIN
  dispatchButton(trip1ResetButton, core::ButtonId::Trip1Reset, nowMs);
#endif
#ifdef ASKOMPU_HAS_FOOT_RESET_PIN
  dispatchButton(footResetButton, core::ButtonId::FootReset, nowMs);
#endif

  if (pulseSnapshot.pendingPulses > 0) {
    Serial.printf("GPIO%u pulses: %lu, total: %lu\n",
                  static_cast<unsigned>(BoardConfig::PIN_PULSE_INPUT),
                  static_cast<unsigned long>(pulseSnapshot.pendingPulses),
                  static_cast<unsigned long>(pulseSnapshot.totalPulses));
  }

  application.handleDistancePulses(
      {pulseSnapshot.pendingPulses, pulseSnapshot.previousPulseAtUs,
       pulseSnapshot.lastPulseAtUs, nowUs, reverseFilter.active()});
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

  domain::DebugDisplaySettings debugSettingsToSave;
  if (application.takeDebugDisplaySettingsSaveRequest(debugSettingsToSave)) {
    application.completeDebugDisplaySettingsSave(
        settingsRepository.saveDebugDisplaySettings(debugSettingsToSave));
  }

  const domain::RouteOrder* orderToSave = nullptr;
  if (application.takeRouteOrderSaveRequest(orderToSave)) {
    const bool saved = orderToSave && routeOrderStore.replace(*orderToSave);
    application.completeRouteOrderSave(saved);
    Serial.printf("Route order save: %s\n", saved ? "ok" : "failed");
  }
  if (application.takeRouteOrderCompletionRequest()) {
    application.completeRouteOrderCompletion(routeOrderStore.markCompleted());
  }

  const core::DisplayModel displayModel = application.displayModel();
  const bool screenChanged = displayModel.screen != renderedScreen;
  if (screenChanged ||
      nowMs - lastDisplayUpdateMs >= BoardConfig::DISPLAY_UPDATE_INTERVAL_MS) {
    view.render(displayModel);
    renderedScreen = displayModel.screen;
    lastDisplayUpdateMs = nowMs;
  }
  delay(POLL_INTERVAL_MS);
}
