#include "DisplayView.h"

#include <cstdio>

#include "BoardConfig.h"

namespace ui {
namespace {

constexpr int16_t TRIP_AREA_HEIGHT = 105;
constexpr int16_t BUTTON_CELL_WIDTH = 64;
constexpr int16_t BUTTON_AREA_HEIGHT =
    BoardConfig::DISPLAY_HEIGHT - TRIP_AREA_HEIGHT;

struct ButtonLabel {
  const char* gpio;
  const char* name;
};

constexpr ButtonLabel BUTTON_LABELS[] = {
    {"GPIO1", "VASEN"}, {"GPIO2", "YLOS"}, {"GPIO3", "ALAS"},
    {"GPIO10", "OIKEA"}, {"GPIO14", "RESET"},
};

uint8_t toIndex(ButtonIndicator indicator) {
  return static_cast<uint8_t>(indicator);
}

}  // namespace

void DisplayView::begin() {
  display_.init();
  display_.setRotation(1);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

  display_.fillScreen(TFT_BLACK);
  display_.drawFastHLine(0, TRIP_AREA_HEIGHT - 1,
                         BoardConfig::DISPLAY_WIDTH, TFT_DARKGREY);
}

void DisplayView::showTrip(uint32_t value) {
  if (value == displayedTrip_) {
    return;
  }
  displayedTrip_ = value;

  char valueText[11];
  std::snprintf(valueText, sizeof(valueText), "%lu",
                static_cast<unsigned long>(value));

  display_.fillRect(0, 0, BoardConfig::DISPLAY_WIDTH,
                    TRIP_AREA_HEIGHT - 1, TFT_BLACK);
  display_.setTextDatum(TC_DATUM);
  display_.setTextSize(1);
  display_.setTextColor(TFT_CYAN, TFT_BLACK);
  display_.drawString("TRIP 1", BoardConfig::DISPLAY_WIDTH / 2, 8, 2);

  display_.setTextDatum(MC_DATUM);
  display_.setTextSize(2);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);
  display_.drawString(valueText, BoardConfig::DISPLAY_WIDTH / 2, 63, 2);
  display_.setTextSize(1);
}

void DisplayView::showButtonState(ButtonIndicator indicator, bool pressed) {
  const uint8_t index = toIndex(indicator);
  if (index >= toIndex(ButtonIndicator::Count)) {
    return;
  }

  if (buttonStateInitialized_[index] && displayedButtonStates_[index] == pressed) {
    return;
  }

  displayedButtonStates_[index] = pressed;
  buttonStateInitialized_[index] = true;
  drawButton(indicator, pressed);
}

void DisplayView::drawButton(ButtonIndicator indicator, bool pressed) {
  const uint8_t index = toIndex(indicator);
  const int16_t x = static_cast<int16_t>(index) * BUTTON_CELL_WIDTH;
  const uint16_t background = pressed ? TFT_DARKGREEN : TFT_DARKGREY;

  display_.fillRect(x + 1, TRIP_AREA_HEIGHT, BUTTON_CELL_WIDTH - 2,
                    BUTTON_AREA_HEIGHT, background);
  display_.setTextDatum(MC_DATUM);
  display_.setTextSize(1);
  display_.setTextColor(TFT_WHITE, background);
  display_.drawString(BUTTON_LABELS[index].gpio, x + BUTTON_CELL_WIDTH / 2,
                      TRIP_AREA_HEIGHT + 10, 1);
  display_.drawString(BUTTON_LABELS[index].name, x + BUTTON_CELL_WIDTH / 2,
                      TRIP_AREA_HEIGHT + 25, 1);
  display_.drawString(pressed ? "LOW" : "HIGH",
                      x + BUTTON_CELL_WIDTH / 2,
                      TRIP_AREA_HEIGHT + 48, 2);
}

}  // namespace ui

