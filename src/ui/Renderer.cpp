#include "Renderer.h"

#include <cstdio>

namespace ui {

void Renderer::drawMain(const domain::Settings& settings,
                        const domain::TripModel& trip,
                        const domain::PointsModel& points,
                        uint32_t effectivePulses) {
  auto pal = domain::paletteForTheme(settings.theme);

  layout_.tft.fillRect(layout_.rectClock.x, layout_.rectClock.y, layout_.rectClock.w,
                       layout_.rectClock.h, pal.bg);
  layout_.tft.fillRect(layout_.rectPoints.x, layout_.rectPoints.y, layout_.rectPoints.w,
                       layout_.rectPoints.h, pal.bg);
  layout_.tft.fillRect(layout_.rectTrip.x, layout_.rectTrip.y, layout_.rectTrip.w,
                       layout_.rectTrip.h, pal.bg);
  layout_.tft.fillRect(layout_.rectStatus.x, layout_.rectStatus.y, layout_.rectStatus.w,
                       layout_.rectStatus.h, pal.bg);

  char buf[24];
  layout_.tft.setTextDatum(TL_DATUM);
  layout_.tft.setTextColor(pal.fg, pal.bg);
  snprintf(buf, sizeof(buf), "%02u:%02u", settings.clock.hours(), settings.clock.minutes());
  layout_.tft.drawString(buf, layout_.rectClock.x + 4, layout_.rectClock.y + 1,
                         layout_.fontSmall);

  snprintf(buf, sizeof(buf), "P %03u", points.value());
  layout_.tft.drawString(buf, layout_.rectPoints.x + 8, layout_.rectPoints.y + 8,
                         layout_.fontHuge);

  snprintf(buf, sizeof(buf), "%04lu m", static_cast<unsigned long>(trip.meters()));
  layout_.tft.drawString(buf, layout_.rectTrip.x + 8, layout_.rectTrip.y + 8,
                         layout_.fontHuge);

  snprintf(buf, sizeof(buf), "Pulssit: %lu", static_cast<unsigned long>(effectivePulses));
  layout_.tft.drawString(buf, layout_.rectStatus.x + 4, layout_.rectStatus.y + 0,
                         layout_.fontSmall);
}

void Renderer::drawClockEditor(uint8_t hh, uint8_t mm, bool editMinutes) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("Kello (kaynnistys)", 10, 8, 4);
  layout_.tft.drawString("LEFT/RIGHT=seuraava", 10, 32, 2);

  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u", hh, mm);
  layout_.tft.drawString(buf, 56, 56, 7);
  layout_.tft.drawString(editMinutes ? "Muokkaa: minuutit" : "Muokkaa: tunnit", 18, 132, 2);
}

void Renderer::drawCoefficientEditor(uint32_t coefficient) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("Kerroin", 16, 12, 4);
  layout_.tft.drawString("LEFT/RIGHT=siirry", 16, 34, 2);
  char buf[20];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(coefficient));
  layout_.tft.drawString(buf, 44, 62, 7);
  layout_.tft.drawString("UP/DOWN muuttaa", 18, 132, 2);
}

void Renderer::drawThemeEditor(domain::Theme theme) {
  layout_.tft.fillScreen(TFT_BLACK);
  const char* names[] = {"Punainen", "Vihrea", "Sininen"};
  uint8_t idx = static_cast<uint8_t>(theme);
  if (idx > 2) idx = 0;
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("Teema", 16, 16, 4);
  layout_.tft.drawString(names[idx], 16, 70, 6);
  layout_.tft.drawString("UP/DOWN vaihtaa", 18, 132, 2);
  layout_.tft.drawString("LEFT/RIGHT=siirry", 18, 150, 2);
}

void Renderer::drawDebug(const domain::Diagnostics& diag, uint32_t coefficient) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("DEBUG", 8, 4, 4);

  char line[40];
  snprintf(line, sizeof(line), "km/h: %.2f", diag.kmh);
  layout_.tft.drawString(line, 8, 30, 2);
  snprintf(line, sizeof(line), "pulssit: %lu", static_cast<unsigned long>(diag.pulseCount));
  layout_.tft.drawString(line, 8, 50, 2);
  snprintf(line, sizeof(line), "ikkuna(200ms): %lu",
           static_cast<unsigned long>(diag.pulsesWindow));
  layout_.tft.drawString(line, 8, 70, 2);
  snprintf(line, sizeof(line), "kerroin: %lu", static_cast<unsigned long>(coefficient));
  layout_.tft.drawString(line, 8, 90, 2);
  snprintf(line, sizeof(line), "loop Hz: %u", diag.loopHz);
  layout_.tft.drawString(line, 8, 110, 2);
  layout_.tft.drawString("LEFT/RIGHT=siirry", 8, 140, 2);
}

}  // namespace ui
