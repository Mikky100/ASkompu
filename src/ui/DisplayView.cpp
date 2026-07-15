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

void DisplayView::render(const core::DisplayModel& model) {
  if (model.screen != displayedScreen_) {
    displayedScreen_ = model.screen;
    if (model.screen == core::Screen::Drive) {
      showDriveScreen();
    } else {
      calibrationInitialized_ = false;
    }
  }

  if (model.screen == core::Screen::Calibration) {
    showCalibration(model.calibration.editedMillimetersPerPulse,
                    model.calibration.saveFailed);
    return;
  }

  showSpeed(model.speedKmh);
  showDiagnostics(model.totalPulseCount, model.trip1.distanceMillimeters,
                  model.trip2.distanceMillimeters);
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
  displayedTotalPulses_ = UINT64_MAX;
  displayedTrip1DistanceMm_ = UINT64_MAX;
  displayedTrip2DistanceMm_ = UINT64_MAX;
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

void DisplayView::showDiagnostics(uint64_t totalPulses,
                                  uint64_t trip1DistanceMm,
                                  uint64_t trip2DistanceMm) {
  if (totalPulses == displayedTotalPulses_ &&
      trip1DistanceMm == displayedTrip1DistanceMm_ &&
      trip2DistanceMm == displayedTrip2DistanceMm_) {
    return;
  }
  displayedTotalPulses_ = totalPulses;
  displayedTrip1DistanceMm_ = trip1DistanceMm;
  displayedTrip2DistanceMm_ = trip2DistanceMm;

  char totalText[24];
  char trip1Text[24];
  char trip2Text[24];
  std::snprintf(totalText, sizeof(totalText), "%" PRIu64, totalPulses);
  const auto formatTrip = [](char* text, size_t size, uint64_t distanceMm) {
    if (distanceMm < 10000000ULL) {
      const uint64_t wholeMeters = distanceMm / 1000ULL;
      std::snprintf(text, size, "%" PRIu64 ".%03" PRIu64,
                    wholeMeters / 1000ULL, wholeMeters % 1000ULL);
    } else {
      const uint64_t wholeTenMeters = distanceMm / 10000ULL;
      std::snprintf(text, size, "%" PRIu64 ".%02" PRIu64,
                    wholeTenMeters / 100ULL, wholeTenMeters % 100ULL);
    }
  };
  formatTrip(trip1Text, sizeof(trip1Text), trip1DistanceMm);
  formatTrip(trip2Text, sizeof(trip2Text), trip2DistanceMm);

  constexpr int16_t columnWidth = BoardConfig::DISPLAY_WIDTH / 3;
  display_.fillRect(0, DIAGNOSTIC_AREA_TOP, BoardConfig::DISPLAY_WIDTH,
                    DIAGNOSTIC_AREA_HEIGHT - 1, TFT_BLACK);
  display_.drawFastVLine(columnWidth, DIAGNOSTIC_AREA_TOP,
                         DIAGNOSTIC_AREA_HEIGHT - 1, TFT_DARKGREY);
  display_.drawFastVLine(columnWidth * 2, DIAGNOSTIC_AREA_TOP,
                         DIAGNOSTIC_AREA_HEIGHT - 1, TFT_DARKGREY);

  display_.setTextSize(1);
  display_.setTextDatum(MC_DATUM);
  display_.setTextColor(TFT_CYAN, TFT_BLACK);
  display_.drawString("TOTAL", columnWidth / 2, DIAGNOSTIC_AREA_TOP + 7, 1);
  display_.drawString("TRIP 1", columnWidth + columnWidth / 2,
                      DIAGNOSTIC_AREA_TOP + 7, 1);
  display_.drawString("TRIP 2", columnWidth * 2 + columnWidth / 2,
                      DIAGNOSTIC_AREA_TOP + 7, 1);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);
  display_.drawString(totalText, columnWidth / 2,
                      DIAGNOSTIC_AREA_TOP + 23, 2);
  display_.drawString(trip1Text, columnWidth + columnWidth / 2,
                      DIAGNOSTIC_AREA_TOP + 23, 2);
  display_.drawString(trip2Text, columnWidth * 2 + columnWidth / 2,
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
