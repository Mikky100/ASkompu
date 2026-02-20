#include "Layout.h"

namespace ui {

Layout::Layout(TFT_eSPI& display) : tft(display), canvas(&display) { recalc(); }

void Layout::recalc() {
  // Muokkaa näitä arvoja, jos näyttö vaihtuu tai haluat eri painotuksen.
  // Kaikki pääruudun elementit viittaavat vain näihin recteihin.
  const int w = tft.width();
  const int h = tft.height();

  rectClock = Rect{0, 0, w, 24};
  rectPoints = Rect{0, 24, w, 40};
  rectTrip = Rect{0, 64, w, 74};
  rectStatus = Rect{0, 138, w, h - 138};
  rectMenu = Rect{10, 20, w - 20, h - 40};
}

}  // namespace ui
