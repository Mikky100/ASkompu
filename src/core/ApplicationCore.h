#pragma once

#include <cstdint>

#include "AppEvent.h"
#include "Clock.h"
#include "DisplayModel.h"
#include "domain/SpeedCalculator.h"
#include "domain/RouteOrder.h"
#include "domain/DisplaySetting.h"
#include "route/RouteOrderEditor.h"

namespace core {

enum class MenuPage : uint8_t {
  Main,
  Results,
  Display,
  TextColor,
  Trips,
  System,
};

class ApplicationCore {
 public:
  ApplicationCore(Clock& clock, uint32_t millimetersPerPulse,
                  uint32_t zeroSpeedTimeoutUs);

  void setInitialMillimetersPerPulse(uint32_t millimetersPerPulse);
  void handleButton(const ButtonEvent& event);
  void handleDistancePulses(const DistancePulseEvent& event);
  void tick(uint32_t nowUs);

  DisplayModel displayModel() const;

  bool takeCalibrationSaveRequest(uint32_t& millimetersPerPulse);
  void completeCalibrationSave(bool succeeded);
  void setInitialRouteOrder(const domain::RouteOrder& order);
  bool takeRouteOrderSaveRequest(const domain::RouteOrder*& order);
  void completeRouteOrderSave(bool succeeded);
  void setInitialTextColor(domain::TextColor color);
  bool takeTextColorSaveRequest(domain::TextColor& color);
  void completeTextColorSave(bool succeeded);
  bool hasRouteOrder() const { return hasRouteOrder_; }
  const domain::RouteOrder* currentRouteOrder() const {
    return hasRouteOrder_ ? &currentRouteOrder_ : nullptr;
  }

  uint64_t trip1DistanceMillimeters() const { return trip1DistanceMm_; }
  uint64_t trip2DistanceMillimeters() const { return trip2DistanceMm_; }
  uint64_t totalPulseCount() const { return totalPulseCount_; }
  uint32_t millimetersPerPulse() const { return millimetersPerPulse_; }
  Screen screen() const { return screen_; }
  MenuPage menuPage() const { return menuPage_; }
  uint8_t menuSelectedIndex() const { return menuSelectedIndex_; }
  uint8_t menuScrollOffset() const { return menuScrollOffset_; }

 private:
  void handleTimeEntry(const ButtonEvent& event);
  void handleBasicView(const ButtonEvent& event);
  void handleMenu(const ButtonEvent& event);
  void handleCalibration(const ButtonEvent& event);
  void handleOrderEditor(const ButtonEvent& event);
  void openMainMenu(uint8_t selectedIndex);
  void openSubmenu(MenuPage page);
  void activateMenuItem();
  void beginTimeEdit(bool startup);
  void resetTrip1();
  void resetTrip2();
  void addDistance(uint64_t incrementMillimeters, uint32_t pulseCount);
  void updateMenuScroll();
  uint8_t menuItemCount() const;
  bool menuWraps() const;

  Clock& clock_;
  domain::SpeedCalculator speedCalculator_;
  uint32_t zeroSpeedTimeoutUs_;
  uint32_t millimetersPerPulse_;
  uint32_t editedMillimetersPerPulse_;
  domain::TextColor textColor_ = domain::TextColor::WHITE;
  domain::TextColor editedTextColor_ = domain::TextColor::WHITE;
  bool textColorSavePending_ = false;
  bool textColorSaveInFlight_ = false;
  uint64_t trip1DistanceMm_ = 0;
  uint64_t trip2DistanceMm_ = 0;
  uint64_t totalPulseCount_ = 0;
  uint64_t trip1PulseCount_ = 0;
  uint64_t trip2PulseCount_ = 0;
  bool trip1ResetHeld_ = false;
  bool calibrationSaveFailed_ = false;
  bool calibrationSavePending_ = false;
  bool calibrationSaveInFlight_ = false;
  bool hasRouteOrder_ = false;
  bool routeOrderSavePending_ = false;
  bool routeOrderSaveInFlight_ = false;
  domain::RouteOrder currentRouteOrder_;
  route::RouteOrderEditor routeOrderEditor_;
  Screen screen_ = Screen::StartupTimeEntry;
  MenuPage menuPage_ = MenuPage::Main;
  uint8_t mainMenuSelectedIndex_ = 0;
  uint8_t menuSelectedIndex_ = 0;
  uint8_t menuScrollOffset_ = 0;
  uint8_t editedHour_ = 0;
  uint8_t editedMinute_ = 0;
  TimeField activeTimeField_ = TimeField::Hour;
  bool startupTimeEdit_ = true;
  bool buttonPressed_[5]{};
  uint8_t lastButtonId_ = 0xFF;
  uint8_t lastButtonEventType_ = 0xFF;
  uint32_t previousPulseAtUs_ = 0;
  uint32_t lastPulseAtUs_ = 0;
  uint32_t lastTickAtUs_ = 0;
};

}  // namespace core
