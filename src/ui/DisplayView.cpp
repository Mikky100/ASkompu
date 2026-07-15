#include "DisplayView.h"

#include <inttypes.h>
#include <cstdio>

#include "BoardConfig.h"

namespace ui {
namespace {

constexpr int16_t SPEED_AREA_HEIGHT = 85;
constexpr int16_t DIAGNOSTIC_AREA_TOP = SPEED_AREA_HEIGHT;
constexpr int16_t DIAGNOSTIC_AREA_HEIGHT = 35;
constexpr int16_t BUTTON_AREA_TOP =
    DIAGNOSTIC_AREA_TOP + DIAGNOSTIC_AREA_HEIGHT;
constexpr int16_t BUTTON_CELL_WIDTH = 64;
constexpr int16_t BUTTON_AREA_HEIGHT =
    BoardConfig::DISPLAY_HEIGHT - BUTTON_AREA_TOP;

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

  showDriveScreen();
}

void DisplayView::showDriveScreen() {
  display_.fillScreen(TFT_BLACK);
  display_.drawFastHLine(0, SPEED_AREA_HEIGHT - 1,
                         BoardConfig::DISPLAY_WIDTH, TFT_DARKGREY);
  display_.drawFastHLine(0, BUTTON_AREA_TOP - 1,
                         BoardConfig::DISPLAY_WIDTH, TFT_DARKGREY);
  display_.drawFastVLine(BoardConfig::DISPLAY_WIDTH / 2,
                         DIAGNOSTIC_AREA_TOP, DIAGNOSTIC_AREA_HEIGHT - 1,
                         TFT_DARKGREY);

  displayedSpeedTenths_ = UINT32_MAX;
  displayedTotalPulses_ = UINT32_MAX;
  displayedTripDistanceMm_ = UINT64_MAX;
  calibrationInitialized_ = false;
  for (uint8_t index = 0; index < toIndex(ButtonIndicator::Count); ++index) {
    buttonStateInitialized_[index] = false;
  }
}

void DisplayView::showSpeed(float speedKmh) {
  const uint32_t speedTenths =
      static_cast<uint32_t>(speedKmh * 10.0F + 0.5F);
  if (speedTenths == displayedSpeedTenths_) {
    return;
  }
  displayedSpeedTenths_ = speedTenths;

  char speedText[20];
  std::snprintf(speedText, sizeof(speedText), "%lu.%lu km/h",
                static_cast<unsigned long>(speedTenths / 10),
                static_cast<unsigned long>(speedTenths % 10));

  display_.fillRect(0, 0, BoardConfig::DISPLAY_WIDTH,
                    SPEED_AREA_HEIGHT - 1, TFT_BLACK);
  display_.setTextDatum(TC_DATUM);
  display_.setTextSize(1);
  display_.setTextColor(TFT_CYAN, TFT_BLACK);
  display_.drawString("NOPEUS", BoardConfig::DISPLAY_WIDTH / 2, 5, 2);

  display_.setTextDatum(MC_DATUM);
  display_.setTextSize(2);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);
  display_.drawString(speedText, BoardConfig::DISPLAY_WIDTH / 2, 52, 2);
  display_.setTextSize(1);
}

void DisplayView::showDiagnostics(uint32_t totalPulses,
                                  uint64_t tripDistanceMm) {
  if (totalPulses == displayedTotalPulses_ &&
      tripDistanceMm == displayedTripDistanceMm_) {
    return;
  }
  displayedTotalPulses_ = totalPulses;
  displayedTripDistanceMm_ = tripDistanceMm;

  char totalText[11];
  char tripText[24];
  std::snprintf(totalText, sizeof(totalText), "%lu",
                static_cast<unsigned long>(totalPulses));
  if (tripDistanceMm < 10000000ULL) {
    const uint64_t wholeMeters = tripDistanceMm / 1000ULL;
    std::snprintf(tripText, sizeof(tripText), "%" PRIu64 ".%03" PRIu64,
                  wholeMeters / 1000ULL, wholeMeters % 1000ULL);
  } else {
    const uint64_t wholeTenMeters = tripDistanceMm / 10000ULL;
    std::snprintf(tripText, sizeof(tripText), "%" PRIu64 ".%02" PRIu64,
                  wholeTenMeters / 100ULL, wholeTenMeters % 100ULL);
  }

  display_.fillRect(0, DIAGNOSTIC_AREA_TOP,
                    BoardConfig::DISPLAY_WIDTH / 2 - 1,
                    DIAGNOSTIC_AREA_HEIGHT - 1, TFT_BLACK);
  display_.fillRect(BoardConfig::DISPLAY_WIDTH / 2 + 1,
                    DIAGNOSTIC_AREA_TOP,
                    BoardConfig::DISPLAY_WIDTH / 2 - 1,
                    DIAGNOSTIC_AREA_HEIGHT - 1, TFT_BLACK);

  display_.setTextSize(1);
  display_.setTextDatum(MC_DATUM);
  display_.setTextColor(TFT_CYAN, TFT_BLACK);
  display_.drawString("TOTAL", BoardConfig::DISPLAY_WIDTH / 4,
                      DIAGNOSTIC_AREA_TOP + 7, 1);
  display_.drawString("TRIP 1", BoardConfig::DISPLAY_WIDTH * 3 / 4,
                      DIAGNOSTIC_AREA_TOP + 7, 1);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);
  display_.drawString(totalText, BoardConfig::DISPLAY_WIDTH / 4,
                      DIAGNOSTIC_AREA_TOP + 23, 2);
  display_.drawString(tripText, BoardConfig::DISPLAY_WIDTH * 3 / 4,
                      DIAGNOSTIC_AREA_TOP + 23, 2);
}

void DisplayView::showCalibration(uint32_t millimetersPerPulse,
                                  bool saveFailed) {
  if (calibrationInitialized_ &&
      millimetersPerPulse == displayedCalibration_ &&
      saveFailed == displayedSaveFailed_) {
    return;
  }
  displayedCalibration_ = millimetersPerPulse;
  displayedSaveFailed_ = saveFailed;
  calibrationInitialized_ = true;

  char valueText[32];
  std::snprintf(valueText, sizeof(valueText), "%lu mm/pulssi",
                static_cast<unsigned long>(millimetersPerPulse));

  display_.fillScreen(TFT_BLACK);
  display_.setTextDatum(TC_DATUM);
  display_.setTextSize(1);
  display_.setTextColor(TFT_CYAN, TFT_BLACK);
  display_.drawString("KALIBROINTI", BoardConfig::DISPLAY_WIDTH / 2, 10, 2);

  display_.setTextDatum(MC_DATUM);
  display_.setTextSize(2);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);
  display_.drawString(valueText, BoardConfig::DISPLAY_WIDTH / 2, 68, 2);

  display_.setTextSize(1);
  display_.setTextColor(saveFailed ? TFT_RED : TFT_LIGHTGREY, TFT_BLACK);
  display_.drawString(saveFailed ? "TALLENNUSVIRHE" : "YLOS/ALAS  +/- 1",
                      BoardConfig::DISPLAY_WIDTH / 2, 112, 1);
  display_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display_.drawString("VASEN PERUUTA   OIKEA TALLENNA",
                      BoardConfig::DISPLAY_WIDTH / 2, 142, 1);
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

  display_.fillRect(x + 1, BUTTON_AREA_TOP, BUTTON_CELL_WIDTH - 2,
                    BUTTON_AREA_HEIGHT, background);
  display_.setTextDatum(MC_DATUM);
  display_.setTextSize(1);
  display_.setTextColor(TFT_WHITE, background);
  display_.drawString(BUTTON_LABELS[index].gpio, x + BUTTON_CELL_WIDTH / 2,
                      BUTTON_AREA_TOP + 7, 1);
  display_.drawString(BUTTON_LABELS[index].name, x + BUTTON_CELL_WIDTH / 2,
                      BUTTON_AREA_TOP + 20, 1);
  display_.drawString(pressed ? "LOW" : "HIGH",
                      x + BUTTON_CELL_WIDTH / 2,
                      BUTTON_AREA_TOP + 38, 1);
}

}  // namespace ui
