#include <Arduino.h>

#include "BoardConfig.h"
#include "DemoConfig.h"
#include "domain/SpeedCalculator.h"
#include "domain/TripCounter.h"
#include "input/DebouncedButton.h"
#include "input/PulseInput.h"
#include "ui/DisplayView.h"

namespace {

constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t POLL_INTERVAL_MS = 10;

input::DebouncedButton leftButton(BoardConfig::PIN_BUTTON_LEFT,
                                  BoardConfig::BUTTON_PRESSED_LEVEL,
                                  DEBOUNCE_MS);
input::DebouncedButton upButton(BoardConfig::PIN_BUTTON_UP,
                                BoardConfig::BUTTON_PRESSED_LEVEL,
                                DEBOUNCE_MS);
input::DebouncedButton downButton(BoardConfig::PIN_BUTTON_DOWN,
                                  BoardConfig::BUTTON_PRESSED_LEVEL,
                                  DEBOUNCE_MS);
input::DebouncedButton rightButton(BoardConfig::PIN_BUTTON_RIGHT,
                                   BoardConfig::BUTTON_PRESSED_LEVEL,
                                   DEBOUNCE_MS);
input::DebouncedButton tripResetButton(BoardConfig::PIN_BUTTON_TRIP_RESET,
                                       BoardConfig::BUTTON_PRESSED_LEVEL,
                                       DEBOUNCE_MS);
input::PulseInput pulseInput(BoardConfig::PIN_PULSE_INPUT,
                             BoardConfig::PULSE_INPUT_MODE,
                             BoardConfig::PULSE_INTERRUPT_MODE);

domain::TripCounter trip1(DemoConfig::millimetersPerPulse);
domain::SpeedCalculator speedCalculator(DemoConfig::millimetersPerPulse,
                                        DemoConfig::zeroSpeedTimeoutUs);
ui::DisplayView view;
uint32_t lastDisplayUpdateMs = 0;

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

}  // namespace

void setup() {
  Serial.begin(115200);

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

  if (nowMs - lastDisplayUpdateMs >= DemoConfig::displayUpdateIntervalMs) {
    lastDisplayUpdateMs = nowMs;
    view.showSpeed(speedCalculator.speedKmh());
    view.showDiagnostics(pulseSnapshot.totalPulses,
                         trip1.distanceMillimeters());
  }
  updateButtonStatesOnDisplay();

  delay(POLL_INTERVAL_MS);
}
