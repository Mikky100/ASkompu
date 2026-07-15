#include "ApplicationCore.h"

#include <cstddef>
#include <limits>

#include "domain/CalibrationSetting.h"
#include "domain/MotionMath.h"

namespace core {
namespace {

struct MenuItem {
  const char* label;
  bool enabled;
};

constexpr MenuItem MAIN_ITEMS[] = {
    {"AJOMAARAYS", true},
    {"KELLO", true},
    {"KERROIN", true},
    {"PISTEET JA TAPAHTUMAT", true},
    {"NAYTTOASETUKSET", true},
    {"TRIPIT", true},
    {"JARJESTELMA", true},
};
constexpr MenuItem RESULT_ITEMS[] = {{"JAKSOJEN PISTEET", false},
                                     {"KOKONAISPISTEET", false},
                                     {"TAPAHTUMAT", false}};
constexpr MenuItem DISPLAY_ITEMS[] = {{"NAYTTOPROFIILI", false},
                                      {"NAYTTOSELITTEET", false},
                                      {"AIKAERON MUOTO", false},
                                      {"TRIP-TARKKUUS", false},
                                      {"TEKSTIN VARI", true}};
constexpr MenuItem TEXT_COLOR_ITEMS[] = {{"VALKOINEN", true},
                                         {"PUNAINEN", true},
                                         {"VIHREA", true}};
constexpr MenuItem TRIP_ITEMS[] = {{"NOLLAA TRIP 1", true},
                                   {"NOLLAA TRIP 2", true},
                                   {"ULKOINEN TRIP", false}};
constexpr MenuItem SYSTEM_ITEMS[] = {{"DIAGNOSTIIKKA", true},
                                     {"PAINIKEASETUKSET", false},
                                     {"MUUT ASETUKSET", false}};

template <std::size_t N>
uint8_t countOf(const MenuItem (&)[N]) {
  return static_cast<uint8_t>(N);
}

const MenuItem* itemsFor(MenuPage page, uint8_t& count, const char*& title) {
  switch (page) {
    case MenuPage::Main:
      title = "PAAVALIKKO";
      count = countOf(MAIN_ITEMS);
      return MAIN_ITEMS;
    case MenuPage::Results:
      title = "PISTEET JA LOKI";
      count = countOf(RESULT_ITEMS);
      return RESULT_ITEMS;
    case MenuPage::Display:
      title = "NAYTTO";
      count = countOf(DISPLAY_ITEMS);
      return DISPLAY_ITEMS;
    case MenuPage::TextColor:
      title = "TEKSTIN VARI";
      count = countOf(TEXT_COLOR_ITEMS);
      return TEXT_COLOR_ITEMS;
    case MenuPage::Trips:
      title = "TRIPIT";
      count = countOf(TRIP_ITEMS);
      return TRIP_ITEMS;
    case MenuPage::System:
      title = "JARJESTELMA";
      count = countOf(SYSTEM_ITEMS);
      return SYSTEM_ITEMS;
  }
  title = "";
  count = 0;
  return nullptr;
}

uint8_t diagnosticButtonIndex(ButtonId id) {
  switch (id) {
    case ButtonId::Left:
      return 0;
    case ButtonId::Up:
      return 1;
    case ButtonId::Down:
      return 2;
    case ButtonId::Right:
      return 3;
    case ButtonId::Trip1Reset:
    case ButtonId::FootReset:
      return 4;
    default:
      return 0xFF;
  }
}

}  // namespace

ApplicationCore::ApplicationCore(Clock& clock, uint32_t millimetersPerPulse,
                                 uint32_t zeroSpeedTimeoutUs)
    : clock_(clock),
      speedCalculator_(
          domain::calibration::validatedOrDefault(millimetersPerPulse),
          zeroSpeedTimeoutUs),
      zeroSpeedTimeoutUs_(zeroSpeedTimeoutUs),
      millimetersPerPulse_(
          domain::calibration::validatedOrDefault(millimetersPerPulse)),
      editedMillimetersPerPulse_(millimetersPerPulse_) {}

void ApplicationCore::setInitialMillimetersPerPulse(
    uint32_t millimetersPerPulse) {
  millimetersPerPulse_ =
      domain::calibration::validatedOrDefault(millimetersPerPulse);
  editedMillimetersPerPulse_ = millimetersPerPulse_;
  speedCalculator_.setMillimetersPerPulse(millimetersPerPulse_);
}

void ApplicationCore::handleButton(const ButtonEvent& event) {
  lastButtonId_ = static_cast<uint8_t>(event.buttonId);
  lastButtonEventType_ = static_cast<uint8_t>(event.eventType);
  const uint8_t diagnosticIndex = diagnosticButtonIndex(event.buttonId);
  if (diagnosticIndex < 5) {
    if (event.eventType == ButtonEventType::Press) {
      buttonPressed_[diagnosticIndex] = true;
    } else if (event.eventType == ButtonEventType::Release) {
      buttonPressed_[diagnosticIndex] = false;
    }
  }

  if (event.buttonId == ButtonId::Trip1Reset ||
      event.buttonId == ButtonId::FootReset) {
    if (event.eventType == ButtonEventType::Press) {
      trip1ResetHeld_ = true;
      resetTrip1();
    } else if (event.eventType == ButtonEventType::Release) {
      trip1ResetHeld_ = false;
    }
    return;
  }
  if (event.buttonId == ButtonId::Trip2Reset &&
      event.eventType == ButtonEventType::Press) {
    resetTrip2();
    return;
  }

  if (event.eventType == ButtonEventType::LongStart &&
      event.buttonId == ButtonId::Left &&
      screen_ == Screen::OrderEdit) {
    handleOrderEditor(event);
    return;
  }
  if (event.eventType == ButtonEventType::LongStart &&
      event.buttonId == ButtonId::Left &&
      screen_ != Screen::StartupTimeEntry) {
    screen_ = Screen::BasicView;
    menuPage_ = MenuPage::Main;
    calibrationSavePending_ = false;
    calibrationSaveFailed_ = false;
    return;
  }

  switch (screen_) {
    case Screen::StartupTimeEntry:
    case Screen::TimeEdit:
      handleTimeEntry(event);
      break;
    case Screen::BasicView:
      handleBasicView(event);
      break;
    case Screen::Menu:
      handleMenu(event);
      break;
    case Screen::CalibrationEdit:
      handleCalibration(event);
      break;
    case Screen::OrderEdit:
      handleOrderEditor(event);
      break;
    case Screen::Diagnostics:
      if (event.eventType == ButtonEventType::Press &&
          event.buttonId == ButtonId::Left) {
        openSubmenu(MenuPage::System);
      }
      break;
  }
}

void ApplicationCore::handleTimeEntry(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  if (step && (event.buttonId == ButtonId::Up ||
               event.buttonId == ButtonId::Down)) {
    const int8_t delta = event.buttonId == ButtonId::Up ? 1 : -1;
    uint8_t& value = activeTimeField_ == TimeField::Hour ? editedHour_
                                                         : editedMinute_;
    const uint8_t modulus = activeTimeField_ == TimeField::Hour ? 24 : 60;
    value = static_cast<uint8_t>((value + modulus + delta) % modulus);
    return;
  }
  if (event.eventType != ButtonEventType::Press) {
    return;
  }
  if (event.buttonId == ButtonId::Right) {
    if (activeTimeField_ == TimeField::Hour) {
      activeTimeField_ = TimeField::Minute;
    } else if (isValidClockTime(editedHour_, editedMinute_)) {
      clock_.set(editedHour_, editedMinute_, 0);
      if (startupTimeEdit_) {
        screen_ = Screen::BasicView;
      } else {
        openMainMenu(1);
      }
    }
  } else if (event.buttonId == ButtonId::Left) {
    if (activeTimeField_ == TimeField::Minute) {
      activeTimeField_ = TimeField::Hour;
    } else if (!startupTimeEdit_) {
      openMainMenu(1);
    }
  }
}

void ApplicationCore::handleBasicView(const ButtonEvent& event) {
  if (event.eventType != ButtonEventType::Press) {
    return;
  }
  if (event.buttonId == ButtonId::Down) {
    openMainMenu(0);
  } else if (event.buttonId == ButtonId::Up) {
    openMainMenu(countOf(MAIN_ITEMS) - 1);
  }
}

void ApplicationCore::handleMenu(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  const uint8_t count = menuItemCount();
  if (step && event.buttonId == ButtonId::Up) {
    if (menuSelectedIndex_ > 0) {
      --menuSelectedIndex_;
    } else if (menuWraps() && count > 0) {
      menuSelectedIndex_ = count - 1;
    }
    updateMenuScroll();
  } else if (step && event.buttonId == ButtonId::Down) {
    if (menuSelectedIndex_ + 1 < count) {
      ++menuSelectedIndex_;
    } else if (menuWraps() && count > 0) {
      menuSelectedIndex_ = 0;
    }
    updateMenuScroll();
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Left) {
    if (menuPage_ == MenuPage::Main) {
      screen_ = Screen::BasicView;
    } else if (menuPage_ == MenuPage::TextColor) {
      openSubmenu(MenuPage::Display);
    } else {
      openMainMenu(mainMenuSelectedIndex_);
    }
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Right) {
    activateMenuItem();
  }
}

void ApplicationCore::handleCalibration(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  if (step && event.buttonId == ButtonId::Up) {
    editedMillimetersPerPulse_ =
        domain::calibration::increment(editedMillimetersPerPulse_);
    calibrationSaveFailed_ = false;
  } else if (step && event.buttonId == ButtonId::Down) {
    editedMillimetersPerPulse_ =
        domain::calibration::decrement(editedMillimetersPerPulse_);
    calibrationSaveFailed_ = false;
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Left) {
    calibrationSavePending_ = false;
    calibrationSaveFailed_ = false;
    openMainMenu(2);
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Right &&
             !calibrationSaveInFlight_) {
    if (editedMillimetersPerPulse_ == millimetersPerPulse_) {
      openMainMenu(2);
    } else {
      calibrationSavePending_ = true;
      calibrationSaveFailed_ = false;
    }
  }
}

void ApplicationCore::handleOrderEditor(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  const bool longPress = event.eventType == ButtonEventType::LongStart &&
                         (event.buttonId == ButtonId::Left ||
                          event.buttonId == ButtonId::Right);
  if (!step && !longPress) return;
  route::EditorKey key;
  switch (event.buttonId) {
    case ButtonId::Up:
      key = route::EditorKey::UP;
      break;
    case ButtonId::Down:
      key = route::EditorKey::DOWN;
      break;
    case ButtonId::Left:
      key = route::EditorKey::LEFT;
      break;
    case ButtonId::Right:
      key = route::EditorKey::RIGHT;
      break;
    default:
      return;
  }
  const route::EditorResult result = routeOrderEditor_.handle(key, longPress);
  if (result == route::EditorResult::EXIT) {
    openMainMenu(0);
  } else if (result == route::EditorResult::SAVE_REQUESTED) {
    routeOrderSavePending_ = true;
  }
}

void ApplicationCore::openMainMenu(uint8_t selectedIndex) {
  screen_ = Screen::Menu;
  menuPage_ = MenuPage::Main;
  menuSelectedIndex_ = selectedIndex < countOf(MAIN_ITEMS) ? selectedIndex : 0;
  mainMenuSelectedIndex_ = menuSelectedIndex_;
  updateMenuScroll();
}

void ApplicationCore::openSubmenu(MenuPage page) {
  screen_ = Screen::Menu;
  menuPage_ = page;
  menuSelectedIndex_ = 0;
  menuScrollOffset_ = 0;
}

void ApplicationCore::activateMenuItem() {
  uint8_t count = 0;
  const char* title = nullptr;
  const MenuItem* items = itemsFor(menuPage_, count, title);
  if (menuSelectedIndex_ >= count || !items[menuSelectedIndex_].enabled) {
    return;
  }
  if (menuPage_ == MenuPage::Main) {
    mainMenuSelectedIndex_ = menuSelectedIndex_;
    if (menuSelectedIndex_ == 0) {
      if (hasRouteOrder_)
        routeOrderEditor_.beginBrowse(currentRouteOrder_);
      else
        routeOrderEditor_.beginCreate();
      screen_ = Screen::OrderEdit;
      return;
    }
    if (menuSelectedIndex_ == 1) {
      beginTimeEdit(false);
      return;
    }
    if (menuSelectedIndex_ == 2) {
      editedMillimetersPerPulse_ = millimetersPerPulse_;
      calibrationSaveFailed_ = false;
      calibrationSavePending_ = false;
      screen_ = Screen::CalibrationEdit;
      return;
    }
    static const MenuPage PAGES[] = {MenuPage::Results, MenuPage::Display,
                                     MenuPage::Trips, MenuPage::System};
    openSubmenu(PAGES[menuSelectedIndex_ - 3]);
    return;
  }
  if (menuPage_ == MenuPage::Display && menuSelectedIndex_ == 4) {
    openSubmenu(MenuPage::TextColor);
    menuSelectedIndex_ = static_cast<uint8_t>(textColor_);
  } else if (menuPage_ == MenuPage::TextColor) {
    editedTextColor_ = static_cast<domain::TextColor>(menuSelectedIndex_);
    textColorSavePending_ = true;
  } else if (menuPage_ == MenuPage::Trips) {
    if (menuSelectedIndex_ == 0) {
      resetTrip1();
    } else if (menuSelectedIndex_ == 1) {
      resetTrip2();
    }
  } else if (menuPage_ == MenuPage::System && menuSelectedIndex_ == 0) {
    screen_ = Screen::Diagnostics;
  }
}

void ApplicationCore::beginTimeEdit(bool startup) {
  startupTimeEdit_ = startup;
  activeTimeField_ = TimeField::Hour;
  if (startup || !clock_.isSet()) {
    editedHour_ = 0;
    editedMinute_ = 0;
  } else {
    const ClockTime current = clock_.now();
    editedHour_ = current.hour;
    editedMinute_ = current.minute;
  }
  screen_ = startup ? Screen::StartupTimeEntry : Screen::TimeEdit;
}

void ApplicationCore::resetTrip1() {
  trip1DistanceMm_ = 0;
  trip1PulseCount_ = 0;
}

void ApplicationCore::resetTrip2() {
  trip2DistanceMm_ = 0;
  trip2PulseCount_ = 0;
}

void ApplicationCore::handleDistancePulses(const DistancePulseEvent& event) {
  const uint64_t distanceIncrement =
      domain::motion::distanceMillimetersForPulses(event.pulseCount,
                                                   millimetersPerPulse_);
  totalPulseCount_ =
      domain::motion::saturatingAdd(totalPulseCount_, event.pulseCount);
  addDistance(distanceIncrement, event.pulseCount);
  if (event.pulseCount > 0) {
    previousPulseAtUs_ = event.previousPulseAtUs;
    lastPulseAtUs_ = event.lastPulseAtUs;
  }
  lastTickAtUs_ = event.observedAtUs;
  const uint32_t speedPulseCount =
      totalPulseCount_ > std::numeric_limits<uint32_t>::max()
          ? std::numeric_limits<uint32_t>::max()
          : static_cast<uint32_t>(totalPulseCount_);
  speedCalculator_.update(event.observedAtUs, speedPulseCount,
                          previousPulseAtUs_, lastPulseAtUs_);
}

void ApplicationCore::tick(uint32_t nowUs) {
  lastTickAtUs_ = nowUs;
  const uint32_t speedPulseCount =
      totalPulseCount_ > std::numeric_limits<uint32_t>::max()
          ? std::numeric_limits<uint32_t>::max()
          : static_cast<uint32_t>(totalPulseCount_);
  speedCalculator_.update(nowUs, speedPulseCount, previousPulseAtUs_,
                          lastPulseAtUs_);
  if (clock_.isSet()) {
    clock_.now();
  }
}

DisplayModel ApplicationCore::displayModel() const {
  DisplayModel model{};
  model.screen = screen_;
  model.textColor = textColor_;
  model.clock = clock_.isSet() ? clock_.now() : ClockTime{0, 0, 0};
  model.speedKmh = speedCalculator_.speedKmh();
  model.trip1 = {trip1DistanceMm_, true};
  model.trip2 = {trip2DistanceMm_, true};
  model.timeEntry = {editedHour_, editedMinute_, activeTimeField_,
                     startupTimeEdit_};
  model.calibration = {editedMillimetersPerPulse_, calibrationSaveFailed_};
  model.order.editor = routeOrderEditor_.view();
  model.order.hasSelectedSegment = false;
  model.order.showsStartTime = false;
  if (screen_ == Screen::OrderEdit &&
      model.order.editor.phase == route::EditorPhase::BROWSE) {
    if (model.order.editor.selectedSegment == 0) {
      model.order.showsStartTime = true;
    } else if (model.order.editor.selectedSegment <=
               routeOrderEditor_.draft().segments.size()) {
      model.order.selectedSegment = routeOrderEditor_.draft().segments[
          model.order.editor.selectedSegment - 1];
      model.order.hasSelectedSegment = true;
    }
  }

  if (screen_ == Screen::Menu) {
    uint8_t count = 0;
    const char* title = nullptr;
    const MenuItem* items = itemsFor(menuPage_, count, title);
    model.menu.title = title;
    model.menu.totalRows = count;
    model.menu.selectedIndex = menuSelectedIndex_;
    model.menu.scrollOffset = menuScrollOffset_;
    model.menu.visibleRowCount =
        count - menuScrollOffset_ < MENU_VISIBLE_ROWS
            ? static_cast<uint8_t>(count - menuScrollOffset_)
            : MENU_VISIBLE_ROWS;
    model.menu.selectedVisibleRow = menuSelectedIndex_ - menuScrollOffset_;
    for (uint8_t index = 0; index < model.menu.visibleRowCount; ++index) {
      const MenuItem& item = items[menuScrollOffset_ + index];
      model.menu.rows[index] = {item.label, item.enabled};
    }
  }

  if (screen_ == Screen::Diagnostics) {
    model.diagnostics.currentScreen = screen_;
    for (uint8_t index = 0; index < 5; ++index) {
      model.diagnostics.buttonPressed[index] = buttonPressed_[index];
    }
    model.diagnostics.lastButtonId = lastButtonId_;
    model.diagnostics.lastButtonEventType = lastButtonEventType_;
    model.diagnostics.totalPulseCount = totalPulseCount_;
    model.diagnostics.trip1PulseCount = trip1PulseCount_;
    model.diagnostics.trip2PulseCount = trip2PulseCount_;
    model.diagnostics.trip1DistanceMillimeters = trip1DistanceMm_;
    model.diagnostics.trip2DistanceMillimeters = trip2DistanceMm_;
    model.diagnostics.millimetersPerPulse = millimetersPerPulse_;
    model.diagnostics.speedKmh = speedCalculator_.speedKmh();
    model.diagnostics.lastPulseAgeMilliseconds =
        totalPulseCount_ == 0
            ? 0
            : static_cast<uint32_t>(lastTickAtUs_ - lastPulseAtUs_) /
                  1000UL;
    model.diagnostics.speedZeroTimedOut =
        totalPulseCount_ > 0 &&
        static_cast<uint32_t>(lastTickAtUs_ - lastPulseAtUs_) >=
            zeroSpeedTimeoutUs_;
    model.diagnostics.clockSet = clock_.isSet();
    model.diagnostics.clock = model.clock;
    model.diagnostics.clockElapsedMilliseconds =
        clock_.elapsedSinceSetMilliseconds();
  }
  return model;
}

bool ApplicationCore::takeCalibrationSaveRequest(
    uint32_t& millimetersPerPulse) {
  if (!calibrationSavePending_) {
    return false;
  }
  calibrationSavePending_ = false;
  calibrationSaveInFlight_ = true;
  millimetersPerPulse = editedMillimetersPerPulse_;
  return true;
}

void ApplicationCore::completeCalibrationSave(bool succeeded) {
  if (!calibrationSaveInFlight_) {
    return;
  }
  calibrationSaveInFlight_ = false;
  if (succeeded) {
    millimetersPerPulse_ = editedMillimetersPerPulse_;
    speedCalculator_.setMillimetersPerPulse(millimetersPerPulse_);
    calibrationSaveFailed_ = false;
    openMainMenu(2);
  } else {
    calibrationSaveFailed_ = true;
  }
}

void ApplicationCore::setInitialTextColor(domain::TextColor color) {
  textColor_ = domain::isValidTextColor(color) ? color : domain::TextColor::WHITE;
  editedTextColor_ = textColor_;
}

bool ApplicationCore::takeTextColorSaveRequest(domain::TextColor& color) {
  if (!textColorSavePending_ || textColorSaveInFlight_) return false;
  textColorSavePending_ = false;
  textColorSaveInFlight_ = true;
  color = editedTextColor_;
  return true;
}

void ApplicationCore::completeTextColorSave(bool succeeded) {
  if (!textColorSaveInFlight_) return;
  textColorSaveInFlight_ = false;
  if (succeeded) textColor_ = editedTextColor_;
  openSubmenu(MenuPage::Display);
  menuSelectedIndex_ = 4;
  updateMenuScroll();
}

void ApplicationCore::setInitialRouteOrder(const domain::RouteOrder& order) {
  if (domain::validateRouteOrder(order) ==
      domain::RouteOrderValidationError::NONE) {
    currentRouteOrder_ = order;
    hasRouteOrder_ = true;
  }
}

bool ApplicationCore::takeRouteOrderSaveRequest(
    const domain::RouteOrder*& order) {
  if (!routeOrderSavePending_ || routeOrderSaveInFlight_) return false;
  routeOrderSavePending_ = false;
  routeOrderSaveInFlight_ = true;
  order = &routeOrderEditor_.draft();
  return true;
}

void ApplicationCore::completeRouteOrderSave(bool succeeded) {
  if (!routeOrderSaveInFlight_) return;
  routeOrderSaveInFlight_ = false;
  if (succeeded) {
    currentRouteOrder_ = routeOrderEditor_.draft();
    hasRouteOrder_ = true;
  }
  routeOrderEditor_.completeSave(succeeded);
}

void ApplicationCore::addDistance(uint64_t incrementMillimeters,
                                  uint32_t pulseCount) {
  if (!trip1ResetHeld_) {
    trip1DistanceMm_ =
        domain::motion::saturatingAdd(trip1DistanceMm_, incrementMillimeters);
    trip1PulseCount_ =
        domain::motion::saturatingAdd(trip1PulseCount_, pulseCount);
  }
  trip2DistanceMm_ =
      domain::motion::saturatingAdd(trip2DistanceMm_, incrementMillimeters);
  trip2PulseCount_ =
      domain::motion::saturatingAdd(trip2PulseCount_, pulseCount);
}

void ApplicationCore::updateMenuScroll() {
  if (menuSelectedIndex_ < menuScrollOffset_) {
    menuScrollOffset_ = menuSelectedIndex_;
  } else if (menuSelectedIndex_ >= menuScrollOffset_ + MENU_VISIBLE_ROWS) {
    menuScrollOffset_ = menuSelectedIndex_ - MENU_VISIBLE_ROWS + 1;
  }
}

uint8_t ApplicationCore::menuItemCount() const {
  uint8_t count = 0;
  const char* title = nullptr;
  itemsFor(menuPage_, count, title);
  return count;
}

bool ApplicationCore::menuWraps() const { return menuPage_ == MenuPage::Main; }

}  // namespace core
