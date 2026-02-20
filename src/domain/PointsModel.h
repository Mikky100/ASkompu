#pragma once

#include <Arduino.h>

namespace domain {

// Kilpailupistelogiikka toteutetaan myöhemmässä versiossa.
// v0.0.1: alustava malli ja rajapinta UI:lle.
class PointsModel {
 public:
  uint16_t value() const { return points_; }
  void setValue(uint16_t points) { points_ = points; }

 private:
  uint16_t points_ = 0;
};

}  // namespace domain
