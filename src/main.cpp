#include <Arduino.h>

#include "BoardConfig.h"
#include "CalibrationConfig.h"
#include "DemoConfig.h"
#include "domain/CalibrationSetting.h"
#include "domain/SpeedCalculator.h"
#include "domain/TripCounter.h"
#include "input/DebouncedButton.h"
#include "input/PulseInput.h"
#include "settings/SettingsRepository.h"
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
                                   DEBOUNCE_MS);
input::DebouncedButton tripResetButton(BoardConfig::PIN_BUTTON_TRIP_RESET,
                                       BoardConfig::BUTTON_PRESSED_LEVEL,
                                       DEBOUNCE_MS);
input::PulseInput pulseInput(BoardConfig::PIN_PULSE_INPUT,
                             BoardConfig::PULSE_INPUT_MODE,
                             BoardConfig::PULSE_INTERRUPT_MODE);

domain::TripCounter trip1(
    CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE);
domain::SpeedCalculator speedCalculator(
    CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE,
    DemoConfig::zeroSpeedTimeoutUs);
settings::SettingsRepository settingsRepository;
ui::DisplayView view;
uint32_t lastDisplayUpdateMs = 0;
uint32_t millimetersPerPulse =
    CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE;
uint32_t editedMillimetersPerPulse =
    CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE;
bool calibrationSaveFailed = false;

enum class Screen : uint8_t {
  Drive,
  Calibration,
};

Screen currentScreen = Screen::Drive;

void updateButtonStatesOnDisplay() {
  view.showButtonState(ui::ButtonIndicator::Left, leftButton.isPressed());
  view.showButtonState(ui::ButtonIndicator::Up, upButton.isPressed());
  view.showButtonState(ui::ButtonIndicator::Down, downButton.isPressed());
  view.showButtonState(ui::ButtonIndicator::Right, rightButton.isPressed());
  view.showButtonState(ui::ButtonIndicator::TripReset,
                       tripResetButton.isPressed());
}

void logPressedEvent(const char* name, bool event) {
  if (event) {
    Serial.printf("Pressed: %s\n", name);
  }
}

void openCalibration(bool incrementValue) {
  editedMillimetersPerPulse =
      incrementValue
          ? domain::calibration::increment(millimetersPerPulse)
          : domain::calibration::decrement(millimetersPerPulse);
  calibrationSaveFailed = false;
  currentScreen = Screen::Calibration;
  view.showCalibration(editedMillimetersPerPulse, false);
}

void returnToDrive(uint32_t nowMs) {
  currentScreen = Screen::Drive;
  view.showDriveScreen();
  lastDisplayUpdateMs = nowMs - DemoConfig::displayUpdateIntervalMs;
}

}  // namespace

void setup() {
  Serial.begin(115200);

  const settings::CalibrationLoadResult calibration =
      settingsRepository.loadCalibration();
  millimetersPerPulse = calibration.millimetersPerPulse;
  trip1.setMillimetersPerPulse(millimetersPerPulse);
  speedCalculator.setMillimetersPerPulse(millimetersPerPulse);
  Serial.printf("Calibration: %lu mm/pulse%s\n",
                static_cast<unsigned long>(millimetersPerPulse),
                calibration.usedDefault ? " (default)" : " (NVS)");

  leftButton.begin();
  upButton.begin();
  downButton.begin();
  rightButton.begin();
  tripResetButton.begin();
  pulseInput.begin();

  view.begin();
  view.showSpeed(0.0F);
  view.showDiagnostics(0, trip1.distanceMillimeters());
  updateButtonStatesOnDisplay();
}

void loop() {
  const uint32_t nowMs = millis();

  leftButton.update(nowMs);
  upButton.update(nowMs);
  downButton.update(nowMs);
  rightButton.update(nowMs);
  tripResetButton.update(nowMs);

  const bool leftPressed = leftButton.consumePressedEvent();
  const bool upPressed = upButton.consumePressedEvent();
  const bool downPressed = downButton.consumePressedEvent();
  const bool rightPressed = rightButton.consumePressedEvent();
  const bool resetPressed = tripResetButton.consumePressedEvent();
  const bool upRepeated = upButton.consumeRepeatEvent();
  const bool downRepeated = downButton.consumeRepeatEvent();
  const input::PulseSnapshot pulseSnapshot = pulseInput.consumeSnapshot();
  const uint32_t nowUs = micros();

  logPressedEvent("GPIO1 VASEN", leftPressed);
  logPressedEvent("GPIO2 YLOS", upPressed);
  logPressedEvent("GPIO3 ALAS", downPressed);
  logPressedEvent("GPIO10 OIKEA", rightPressed);
  logPressedEvent("GPIO14 RESET", resetPressed);
  if (pulseSnapshot.pendingPulses > 0) {
    Serial.printf("GPIO16 pulses: %lu, total: %lu\n",
                  static_cast<unsigned long>(pulseSnapshot.pendingPulses),
                  static_cast<unsigned long>(pulseSnapshot.totalPulses));
  }

  if (tripResetButton.isPressed()) {
    trip1.reset();
  } else {
    trip1.addPulses(pulseSnapshot.pendingPulses);
  }

  speedCalculator.update(nowUs, pulseSnapshot.totalPulses,
                         pulseSnapshot.previousPulseAtUs,
                         pulseSnapshot.lastPulseAtUs);

  if (currentScreen == Screen::Drive) {
    if (upPressed) {
      openCalibration(true);
    } else if (downPressed) {
      openCalibration(false);
    }
  } else {
    if (upPressed || upRepeated) {
      editedMillimetersPerPulse =
          domain::calibration::increment(editedMillimetersPerPulse);
      calibrationSaveFailed = false;
    }
    if (downPressed || downRepeated) {
      editedMillimetersPerPulse =
          domain::calibration::decrement(editedMillimetersPerPulse);
      calibrationSaveFailed = false;
    }

    if (leftPressed) {
      returnToDrive(nowMs);
    } else if (rightPressed) {
      if (settingsRepository.saveCalibration(editedMillimetersPerPulse)) {
        millimetersPerPulse = editedMillimetersPerPulse;
        trip1.setMillimetersPerPulse(millimetersPerPulse);
        speedCalculator.setMillimetersPerPulse(millimetersPerPulse);
        Serial.printf("Saved calibration: %lu mm/pulse\n",
                      static_cast<unsigned long>(millimetersPerPulse));
        returnToDrive(nowMs);
      } else {
        calibrationSaveFailed = true;
      }
    }

    if (currentScreen == Screen::Calibration) {
      view.showCalibration(editedMillimetersPerPulse,
                           calibrationSaveFailed);
    }
  }

  if (currentScreen == Screen::Drive) {
    if (nowMs - lastDisplayUpdateMs >= DemoConfig::displayUpdateIntervalMs) {
      lastDisplayUpdateMs = nowMs;
      view.showSpeed(speedCalculator.speedKmh());
      view.showDiagnostics(pulseSnapshot.totalPulses,
                           trip1.distanceMillimeters());
    }
    updateButtonStatesOnDisplay();
  }

  delay(POLL_INTERVAL_MS);
}
