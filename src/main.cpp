#include <Arduino.h>

#include "BoardConfig.h"
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

domain::TripCounter trip1;
ui::DisplayView view;

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
  view.showTrip(trip1.value());
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
  const uint32_t pulses = pulseInput.consumePulses();

  logPressedEvent("GPIO1 VASEN", leftPressed);
  logPressedEvent("GPIO2 YLOS", upPressed);
  logPressedEvent("GPIO3 ALAS", downPressed);
  logPressedEvent("GPIO10 OIKEA", rightPressed);
  logPressedEvent("GPIO14 RESET", resetPressed);
  if (pulses > 0) {
    Serial.printf("GPIO16 pulses: %lu\n", static_cast<unsigned long>(pulses));
  }

  if (resetPressed) {
    trip1.reset();
  } else {
    if (rightPressed) {
      trip1.incrementTestStep();
    }
    trip1.addTestSteps(pulses);
  }

  view.showTrip(trip1.value());
  updateButtonStatesOnDisplay();

  delay(POLL_INTERVAL_MS);
}
