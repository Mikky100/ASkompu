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
  clearWithTheme(settings.theme);

  char buf[24];
  layout_.tft.setTextDatum(TL_DATUM);
  layout_.tft.setTextColor(pal.accent, pal.bg);
  snprintf(buf, sizeof(buf), "%02u:%02u", settings.clock.hours(), settings.clock.minutes());
  layout_.tft.drawString(buf, layout_.rectClock.x + 4, layout_.rectClock.y + 2, layout_.fontLarge);

  layout_.tft.setTextColor(pal.fg, pal.bg);
  layout_.tft.drawString("Pisteet: 000", layout_.rectPoints.x + 4, layout_.rectPoints.y + 4,
                         layout_.fontLarge);

  snprintf(buf, sizeof(buf), "%04lu m", static_cast<unsigned long>(trip.meters()));
  layout_.tft.drawString(buf, layout_.rectTrip.x + 2, layout_.rectTrip.y + 4,
                         layout_.fontHuge);

  snprintf(buf, sizeof(buf), "Pulssit: %lu", static_cast<unsigned long>(effectivePulses));
  layout_.tft.drawString(buf, layout_.rectStatus.x + 4, layout_.rectStatus.y + 2,
                         layout_.fontSmall);
}

void Renderer::drawMenu(uint8_t selected) {
  layout_.tft.fillScreen(TFT_BLACK);
  const char* items[] = {"N\x84ytt\x94", "Asetukset", "Debug"};
  layout_.tft.setTextDatum(TL_DATUM);
  layout_.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  layout_.tft.drawString("Valikko", 16, 16, 4);
  for (int i = 0; i < 3; ++i) {
    uint16_t bg = (selected == i) ? TFT_DARKGREY : TFT_BLACK;
    layout_.tft.fillRect(16, 52 + i * 34, layout_.tft.width() - 32, 28, bg);
    layout_.tft.drawString(items[i], 24, 56 + i * 34, 4);
  }
}

void Renderer::drawSettingsMenu(uint8_t selected) {
  layout_.tft.fillScreen(TFT_BLACK);
  const char* items[] = {"Kello", "Kerroin", "Teema"};
  layout_.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  layout_.tft.drawString("Asetukset", 16, 16, 4);
  for (int i = 0; i < 3; ++i) {
    uint16_t bg = (selected == i) ? TFT_DARKGREY : TFT_BLACK;
    layout_.tft.fillRect(16, 52 + i * 34, layout_.tft.width() - 32, 28, bg);
    layout_.tft.drawString(items[i], 24, 56 + i * 34, 4);
  }
}

void Renderer::drawClockEditor(uint8_t hh, uint8_t mm, bool editMinutes) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  layout_.tft.drawString("Kellon asetus", 12, 10, 4);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u", hh, mm);
  layout_.tft.setTextColor(TFT_CYAN, TFT_BLACK);
  layout_.tft.drawString(buf, 54, 58, 8);
  layout_.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  layout_.tft.drawString(editMinutes ? "Muokkaa: minuutit" : "Muokkaa: tunnit", 18, 132, 2);
}

void Renderer::drawCoefficientEditor(uint32_t coefficient) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  layout_.tft.drawString("Matkakerroin", 16, 16, 4);
  char buf[20];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(coefficient));
  layout_.tft.setTextColor(TFT_CYAN, TFT_BLACK);
  layout_.tft.drawString(buf, 48, 70, 8);
  layout_.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  layout_.tft.drawString("UP/DOWN muuttaa", 18, 132, 2);
}

void Renderer::drawThemeEditor(domain::Theme theme) {
  layout_.tft.fillScreen(TFT_BLACK);
  const char* names[] = {"Night Blue", "Amber", "Neon"};
  uint8_t idx = static_cast<uint8_t>(theme);
  if (idx > 2) idx = 0;
  layout_.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  layout_.tft.drawString("Teema", 16, 16, 4);
  layout_.tft.drawString(names[idx], 16, 70, 6);
  layout_.tft.drawString("UP/DOWN vaihtaa", 18, 132, 2);
}

void Renderer::drawDebug(const domain::Diagnostics& diag, uint32_t coefficient) {
  layout_.tft.fillScreen(TFT_BLACK);
  layout_.tft.setTextColor(TFT_RED, TFT_BLACK);
  layout_.tft.drawString("DEBUG", 8, 4, 4);
  layout_.tft.setTextColor(TFT_WHITE, TFT_BLACK);

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
