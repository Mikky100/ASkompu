#include "Layout.h"

namespace ui {

Layout::Layout(TFT_eSPI& display) : tft(display), canvas(&display) { recalc(); }

void Layout::recalc() {
  // Muokkaa näitä arvoja, jos näyttö vaihtuu tai haluat eri painotuksen.
  // Kaikki pääruudun elementit viittaavat vain näihin recteihin.
  const int16_t w = static_cast<int16_t>(tft.width());
  const int16_t h = static_cast<int16_t>(tft.height());

  // 320x170: pidetään kello pienenä, varataan pisteille ja tripille iso alue.
  rectClock = {0, 0, w, 20};
  rectPoints = {0, 20, w, 48};
  rectTrip = {0, 68, w, 74};
  rectStatus = {0, 142, w, static_cast<int16_t>(h - 142)};
  rectMenu = {10, 10, static_cast<int16_t>(w - 20), static_cast<int16_t>(h - 20)};

  fontHuge = 6;
  fontLarge = 4;
  fontSmall = 2;
}

}  // namespace ui
