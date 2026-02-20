#include "Renderer.h"

#include <cstdio>

namespace ui {

void Renderer::clearWithTheme(domain::Theme theme) {
  auto pal = domain::paletteForTheme(theme);
  layout_.tft.fillScreen(pal.bg);
  layout_.tft.setTextColor(pal.fg, pal.bg);
}

void Renderer::drawMain(const domain::Settings& settings,
                        const domain::TripModel& trip,
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

  layout_.tft.setTextColor(pal.fg, pal.bg);
  layout_.tft.drawString("P 000", layout_.rectPoints.x + 8, layout_.rectPoints.y + 8,
                         layout_.fontHuge);

  snprintf(buf, sizeof(buf), "%04lu m", static_cast<unsigned long>(trip.meters()));
  layout_.tft.drawString(buf, layout_.rectTrip.x + 8, layout_.rectTrip.y + 8,
                         layout_.fontHuge);

  snprintf(buf, sizeof(buf), "Pulssit: %lu", static_cast<unsigned long>(effectivePulses));
  layout_.tft.drawString(buf, layout_.rectStatus.x + 4, layout_.rectStatus.y + 0,
                         layout_.fontSmall);
}

void Renderer::drawMenu(uint8_t selected) {
  layout_.tft.fillScreen(TFT_BLACK);
  const char* items[] = {"N\x84ytt\x94", "Asetukset", "Debug"};
  layout_.tft.setTextDatum(TL_DATUM);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("Valikko", 16, 12, 4);
  for (int i = 0; i < 3; ++i) {
    uint16_t bg = (selected == i) ? TFT_DARKGREY : TFT_BLACK;
    layout_.tft.fillRect(16, 44 + i * 36, layout_.tft.width() - 32, 30, bg);
    layout_.tft.drawString(items[i], 24, 50 + i * 36, 4);
  }
}

void Renderer::drawSettingsMenu(uint8_t selected) {
  layout_.tft.fillScreen(TFT_BLACK);
  const char* items[] = {"Kello", "Kerroin", "Teema"};
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("Asetukset", 16, 12, 4);
  for (int i = 0; i < 3; ++i) {
    uint16_t bg = (selected == i) ? TFT_DARKGREY : TFT_BLACK;
    layout_.tft.fillRect(16, 44 + i * 36, layout_.tft.width() - 32, 30, bg);
    layout_.tft.drawString(items[i], 24, 50 + i * 36, 4);
  }
}

void Renderer::drawClockEditor(uint8_t hh, uint8_t mm, bool editMinutes) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("Aseta kello", 12, 8, 4);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u", hh, mm);
  layout_.tft.drawString(buf, 56, 56, 7);
  layout_.tft.drawString(editMinutes ? "Muokkaa: minuutit" : "Muokkaa: tunnit", 18, 132, 2);
}

void Renderer::drawCoefficientEditor(uint32_t coefficient) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("Matkakerroin", 16, 12, 4);
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
}

void Renderer::drawDebug(const domain::Diagnostics& diag, uint32_t coefficient) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("DEBUG", 8, 4, 4);

  char line[40];
  snprintf(line, sizeof(line), "km/h: %.2f", diag.kmh);
  layout_.tft.drawString(line, 8, 32, 2);
  snprintf(line, sizeof(line), "pulssit: %lu", static_cast<unsigned long>(diag.pulseCount));
  layout_.tft.drawString(line, 8, 54, 2);
  snprintf(line, sizeof(line), "ikkuna(200ms): %lu",
           static_cast<unsigned long>(diag.pulsesWindow));
  layout_.tft.drawString(line, 8, 76, 2);
  snprintf(line, sizeof(line), "kerroin: %lu", static_cast<unsigned long>(coefficient));
  layout_.tft.drawString(line, 8, 98, 2);
  snprintf(line, sizeof(line), "loop Hz: %u", diag.loopHz);
  layout_.tft.drawString(line, 8, 120, 2);
}

}  // namespace ui
