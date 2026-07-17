#pragma once

#include <cstdint>

#include "AppEvent.h"
#include "Clock.h"
#include "DisplayModel.h"
#include "domain/SpeedCalculator.h"
#include "domain/CompetitionEngine.h"
#include "domain/RouteOrder.h"
#include "domain/DisplaySetting.h"
#include "domain/EventRepository.h"
#include "route/RouteOrderEditor.h"

namespace core {

enum class MenuPage : uint8_t {
  Main,
  Results,
  Display,
  TextColor,
  Trips,
  System,
  Debug,
};

class ApplicationCore {
 public:
  ApplicationCore(Clock& clock, uint32_t millimetersPerPulse,
                  uint32_t zeroSpeedTimeoutUs);

  void setInitialMillimetersPerPulse(uint32_t millimetersPerPulse);
  void handleButton(const ButtonEvent& event);
  void handleDistancePulses(const DistancePulseEvent& event);
  void handleReverseSignal(const ReverseSignalEvent& event);
  void tick(uint32_t nowUs);

  DisplayModel displayModel() const;

  bool takeCalibrationSaveRequest(uint32_t& millimetersPerPulse);
  void completeCalibrationSave(bool succeeded);
  void setInitialRouteOrder(const domain::RouteOrder& order,
                            bool active = false);
  bool takeRouteOrderSaveRequest(const domain::RouteOrder*& order);
  void completeRouteOrderSave(bool succeeded);
  bool takeRouteOrderCompletionRequest();
  void completeRouteOrderCompletion(bool succeeded);
  void setInitialTextColor(domain::TextColor color);
  void setCompetitionSettings(const domain::CompetitionSettings& settings);
  bool takeTextColorSaveRequest(domain::TextColor& color);
  void completeTextColorSave(bool succeeded);
  void setInitialDebugDisplaySettings(
      const domain::DebugDisplaySettings& settings);
  bool takeDebugDisplaySettingsSaveRequest(
      domain::DebugDisplaySettings& settings);
  void completeDebugDisplaySettingsSave(bool succeeded);
  bool hasRouteOrder() const { return hasRouteOrder_; }
  const domain::RouteOrder* currentRouteOrder() const {
    return hasRouteOrder_ ? &currentRouteOrder_ : nullptr;
  }

  int64_t trip1DistanceMillimeters() const { return trip1DistanceMm_; }
  int64_t trip2DistanceMillimeters() const { return trip2DistanceMm_; }
  uint64_t totalPulseCount() const { return totalPulseCount_; }
  uint32_t millimetersPerPulse() const { return millimetersPerPulse_; }
  Screen screen() const { return screen_; }
  MenuPage menuPage() const { return menuPage_; }
  uint8_t menuSelectedIndex() const { return menuSelectedIndex_; }
  uint8_t menuScrollOffset() const { return menuScrollOffset_; }
  const domain::CompetitionEngine& competition() const { return competition_; }
  const domain::EventRepository& eventRepository() const {
    return eventRepository_;
  }

 private:
  void handleTimeEntry(const ButtonEvent& event);
  void handleBasicView(const ButtonEvent& event);
  void handleMenu(const ButtonEvent& event);
  void handleCalibration(const ButtonEvent& event);
  void handleMittisProposal(const ButtonEvent& event);
  void completeMittisPoint(domain::PointResult result,
                           uint16_t endedSegment);
  void handleJatResult(const ButtonEvent& event);
  void handleStartTimeEdit(const ButtonEvent& event);
  void beginJatStartTimeEdit();
  void acceptJatStartTime();
  void handleOverrideMenu(const ButtonEvent& event);
  void handleOverrideEdit(const ButtonEvent& event);
  void beginOverrideEdit();
  void acceptOverride();
  void handleResultView(const ButtonEvent& event);
  void handleOrderAccessPrompt(const ButtonEvent& event);
  void handleOrderEditor(const ButtonEvent& event);
  void openMainMenu(uint8_t selectedIndex);
  void openSubmenu(MenuPage page);
  void activateMenuItem();
  void beginTimeEdit(bool startup);
  void resetTrip1();
  void resetTrip2();
  void addDistance(int64_t deltaMillimeters, uint32_t pulseCount);
  domain::EventRecord makeEvent(domain::DomainEventType type) const;
  uint64_t appendEvent(domain::EventRecord record);
  void handleAtRelease();
  void activateCurrentRouteOrder();
  void updateMenuScroll();
  uint8_t menuItemCount() const;
  bool menuWraps() const;

  Clock& clock_;
  domain::SpeedCalculator speedCalculator_;
  domain::CompetitionEngine competition_;
  domain::RamEventRepository eventRepository_;
  domain::CompetitionSettings competitionSettings_;
  uint32_t zeroSpeedTimeoutUs_;
  uint32_t millimetersPerPulse_;
  uint32_t editedMillimetersPerPulse_;
  domain::TextColor textColor_ = domain::TextColor::WHITE;
  domain::TextColor editedTextColor_ = domain::TextColor::WHITE;
  bool textColorSavePending_ = false;
  bool textColorSaveInFlight_ = false;
  domain::DebugDisplaySettings debugDisplaySettings_{};
  domain::DebugDisplaySettings editedDebugDisplaySettings_{};
  bool debugDisplaySavePending_ = false;
  bool debugDisplaySaveInFlight_ = false;
  int64_t trip1DistanceMm_ = 0;
  int64_t trip2DistanceMm_ = 0;
  uint64_t totalPulseCount_ = 0;
  uint64_t trip1PulseCount_ = 0;
  uint64_t trip2PulseCount_ = 0;
  bool trip1ResetHeld_ = false;
  bool calibrationSaveFailed_ = false;
  bool calibrationSavePending_ = false;
  bool calibrationSaveInFlight_ = false;
  bool mittisCalibrationSave_ = false;
  bool hasRouteOrder_ = false;
  bool routeOrderSavePending_ = false;
  bool routeOrderSaveInFlight_ = false;
  bool loadedRouteOrderActive_ = false;
  bool routeOrderCompletionPending_ = false;
  bool routeOrderCompletionInFlight_ = false;
  domain::RouteOrder currentRouteOrder_;
  route::RouteOrderEditor routeOrderEditor_;
  OrderAccessAction orderAccessAction_ = OrderAccessAction::Edit;
  Screen screen_ = Screen::StartupTimeEntry;
  MenuPage menuPage_ = MenuPage::Main;
  uint8_t mainMenuSelectedIndex_ = 0;
  uint8_t menuSelectedIndex_ = 0;
  uint8_t menuScrollOffset_ = 0;
  uint8_t editedHour_ = 0;
  uint8_t editedMinute_ = 0;
  TimeField activeTimeField_ = TimeField::Hour;
  bool startupTimeEdit_ = true;
  bool buttonPressed_[8]{};
  uint8_t lastButtonId_ = 0xFF;
  uint8_t lastButtonEventType_ = 0xFF;
  uint8_t lastPointEventType_ = 0xFF;
  uint8_t lastAtEventType_ = 0xFF;
  bool reverseActive_ = false;
  bool atOverlayVisible_ = false;
  uint64_t lastPointEventId_ = 0;
  uint64_t atEventId_ = 0;
  bool atEventCancellable_ = false;
  uint64_t atEventMonotonicMs_ = 0;
  domain::EventClockTime atClockTime_;
  bool atLowSpeedTiming_ = false;
  uint64_t atLowSpeedSinceMs_ = 0;
  bool atDistanceArmed_ = false;
  int64_t atTravelledDistanceMm_ = 0;
  domain::EventClockTime editedStageStartTime_;
  domain::EventClockTime originalStageStartProposal_;
  int8_t emitOffsetAdjustmentMinutes_ = 0;
  bool stageStartProposalExpired_ = false;
  bool startTimeCorrection_ = false;
  OverrideMenuAction overrideMenuAction_ = OverrideMenuAction::AdditionalOrder;
  OverrideEditPhase overrideEditPhase_ = OverrideEditPhase::StartSegment;
  uint16_t overrideStartSegmentIndex_ = 0;
  uint16_t overrideEndPointIndex_ = 0;
  uint16_t overrideMaximumEndPointIndex_ = 0;
  uint16_t overrideDurationSeconds_ = 60;
  bool overrideInvalid_ = false;
  ResultViewType resultViewType_ = ResultViewType::StageResults;
  uint16_t resultSelectedIndex_ = 0;
  uint32_t previousPulseAtUs_ = 0;
  uint32_t lastPulseAtUs_ = 0;
  uint32_t lastTickAtUs_ = 0;
};

}  // namespace core
