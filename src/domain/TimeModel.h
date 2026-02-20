#pragma once

#include <Arduino.h>

namespace domain {

class TimeModel {
 public:
  void set(uint8_t hh, uint8_t mm) {
    hours_ = hh % 24;
    minutes_ = mm % 60;
    lastTickMs_ = millis();
  }

  void update() {
    unsigned long now = millis();
    while (now - lastTickMs_ >= 60000UL) {
      lastTickMs_ += 60000UL;
      incrementMinute();
    }
  }

  uint8_t hours() const { return hours_; }
  uint8_t minutes() const { return minutes_; }

 private:
  void incrementMinute() {
    minutes_++;
    if (minutes_ >= 60) {
      minutes_ = 0;
      hours_ = (hours_ + 1) % 24;
    }
  }

  uint8_t hours_ = 12;
  uint8_t minutes_ = 0;
  unsigned long lastTickMs_ = 0;
};

}  // namespace domain
