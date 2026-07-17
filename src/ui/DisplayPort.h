#pragma once

#include "core/DisplayModel.h"

namespace ui {

class DisplayPort {
 public:
  virtual ~DisplayPort() = default;
  virtual bool begin() = 0;
  virtual void setBacklight(bool enabled) = 0;
  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;
  virtual void render(const core::DisplayModel& model) = 0;
};

}  // namespace ui
