#include "Layout.h"

namespace ui {

Layout::Layout(TFT_eSPI& display) : tft(display), canvas(&display) { recalc(); }

void Layout::recalc() {
  // Muokkaa näitä arvoja, jos näyttö vaihtuu tai haluat eri painotuksen.
  // Kaikki pääruudun elementit viittaavat vain näihin recteihin.
  const int16_t w = static_cast<int16_t>(tft.width());
  const int16_t h = static_cast<int16_t>(tft.height());

  rectClock = {0, 0, w, 24};
  rectPoints = {0, 24, w, 40};
  rectTrip = {0, 64, w, 74};
  rectStatus = {0, 138, w, static_cast<int16_t>(h - 138)};
  rectMenu = {10, 20, static_cast<int16_t>(w - 20), static_cast<int16_t>(h - 40)};
}

}  // namespace ui
