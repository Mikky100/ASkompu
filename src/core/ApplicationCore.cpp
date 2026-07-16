#include "ApplicationCore.h"

#include <cstddef>
#include <limits>

#include "domain/CalibrationSetting.h"
#include "domain/MotionMath.h"

namespace core {
namespace {

domain::EventClockTime eventClock(const ClockTime& value) {
  return {value.hour, value.minute, value.second};
}

int64_t signedMagnitude(int64_t value) {
  if (value == std::numeric_limits<int64_t>::min())
    return std::numeric_limits<int64_t>::max();
  return value < 0 ? -value : value;
}

uint32_t millisecondsOfDay(const ClockTime& value) {
  return (static_cast<uint32_t>(value.hour) * 3600UL +
          static_cast<uint32_t>(value.minute) * 60UL + value.second) *
         1000UL;
}

domain::EventClockTime addClockMinutes(domain::EventClockTime value,
                                       int8_t minutes) {
  int32_t total = static_cast<int32_t>(value.hour) * 60 + value.minute + minutes;
  total %= 24 * 60;
  if (total < 0) total += 24 * 60;
  value.hour = static_cast<uint8_t>(total / 60);
  value.minute = static_cast<uint8_t>(total % 60);
  value.second = 0;
  return value;
}

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
constexpr MenuItem RESULT_ITEMS[] = {{"JAKSOJEN PISTEET", true},
                                     {"KOKONAISPISTEET", true},
                                     {"TAPAHTUMAT", true}};
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
    case ButtonId::Point:
      return 5;
    case ButtonId::At:
      return 6;
    case ButtonId::Trip2Reset:
      return 7;
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
  if (diagnosticIndex < 8) {
    if (event.eventType == ButtonEventType::Press) {
      buttonPressed_[diagnosticIndex] = true;
    } else if (event.eventType == ButtonEventType::Release) {
      buttonPressed_[diagnosticIndex] = false;
    }
  }
  if (event.buttonId == ButtonId::Point)
    lastPointEventType_ = static_cast<uint8_t>(event.eventType);
  if (event.buttonId == ButtonId::At)
    lastAtEventType_ = static_cast<uint8_t>(event.eventType);

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

  if (event.buttonId == ButtonId::Point &&
      event.eventType == ButtonEventType::Release &&
      screen_ == Screen::StartTimeEdit &&
      !startTimeCorrection_ &&
      competition_.pendingJatType() == domain::JatType::EMIT_JAT_OFFSET) {
    acceptJatStartTime();
    return;
  }

  if (event.buttonId == ButtonId::Point &&
      event.eventType == ButtonEventType::Release &&
      screen_ == Screen::BasicView) {
    const uint16_t endedSegment = competition_.currentSegmentIndex();
    const domain::PointResult result =
        competition_.pointReleased(clock_.elapsedSinceSetMilliseconds(),
                                   millimetersPerPulse_, eventClock(clock_.now()),
                                   competitionSettings_);
    if (result == domain::PointResult::FINISHED) {
      domain::EventRecord record = makeEvent(domain::DomainEventType::FINISH);
      record.segmentIndex = endedSegment;
      const std::vector<domain::StageResult>& results =
          competition_.stageResults();
      if (!results.empty()) {
        record.stageIndex = results.back().stageIndex;
        record.payload.hasStageResult = true;
        record.payload.stageResult = results.back();
        record.payload.finalDeltaMs = competition_.finalDeltaMs();
      }
      appendEvent(record);
      routeOrderCompletionPending_ = true;
    } else if (result == domain::PointResult::MITTIS_PROPOSAL) {
      domain::EventRecord record =
          makeEvent(domain::DomainEventType::MITTIS_PROPOSED);
      record.segmentIndex = endedSegment;
      record.payload.oldCalibration = millimetersPerPulse_;
      record.payload.proposedCalibration =
          competition_.mittisProposal().proposedMillimetersPerPulse;
      appendEvent(record);
      calibrationSaveFailed_ = false;
      screen_ = Screen::MittisProposal;
    } else if (result == domain::PointResult::ADVANCED) {
      domain::EventRecord record =
          makeEvent(domain::DomainEventType::NORMAL_POINT);
      record.segmentIndex = endedSegment;
      lastPointEventId_ = appendEvent(record);
    } else if (result == domain::PointResult::JAT_COMPLETED) {
      domain::EventRecord record = makeEvent(domain::DomainEventType::JAT);
      record.segmentIndex = endedSegment;
      record.payload.arrivalClockTime = competition_.arrivalClockTime();
      record.payload.finalDeltaMs = competition_.finalDeltaMs();
      record.payload.hasJatType = true;
      record.payload.jatType = competition_.pendingJatType();
      const std::vector<domain::StageResult>& results =
          competition_.stageResults();
      if (!results.empty()) {
        record.stageIndex = results.back().stageIndex;
        record.payload.hasStageResult = true;
        record.payload.stageResult = results.back();
      }
      appendEvent(record);
      resetTrip1();
      if (competition_.state() == domain::CompetitionState::EDIT_START_TIME)
        beginJatStartTimeEdit();
      else
        screen_ = Screen::JatResult;
    }
    return;
  }
  if (event.buttonId == ButtonId::Point &&
      event.eventType == ButtonEventType::LongStart) {
    if (screen_ == Screen::BasicView &&
        competition_.state() == domain::CompetitionState::RUNNING) {
      overrideMenuAction_ = OverrideMenuAction::AdditionalOrder;
      screen_ = Screen::OverrideMenu;
    }
    return;
  }
  if (event.buttonId == ButtonId::At &&
      event.eventType == ButtonEventType::Release) {
    handleAtRelease();
    return;
  }
  if (event.buttonId == ButtonId::Left &&
      event.eventType == ButtonEventType::Press &&
      screen_ == Screen::BasicView &&
      competition_.undoPoint(clock_.elapsedSinceSetMilliseconds())) {
    if (lastPointEventId_ != 0)
      eventRepository_.markCancelled(lastPointEventId_);
    domain::EventRecord undo = makeEvent(domain::DomainEventType::POINT_UNDO);
    undo.payload.referencedEventId = lastPointEventId_;
    appendEvent(undo);
    return;
  }
  if (event.buttonId == ButtonId::Left &&
      event.eventType == ButtonEventType::LongStart &&
      screen_ == Screen::MittisProposal) {
    handleMittisProposal(
        {ButtonId::Left, ButtonEventType::Press, event.monotonicMs});
    return;
  }
  if (event.buttonId == ButtonId::Left &&
      event.eventType == ButtonEventType::LongStart &&
      screen_ == Screen::StartTimeEdit) {
    handleStartTimeEdit(
        {ButtonId::Left, ButtonEventType::Press, event.monotonicMs});
    return;
  }
  if (event.buttonId == ButtonId::Left &&
      event.eventType == ButtonEventType::LongStart &&
      screen_ == Screen::JatResult)
    return;

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
    case Screen::MittisProposal:
      handleMittisProposal(event);
      break;
    case Screen::JatResult:
      handleJatResult(event);
      break;
    case Screen::StartTimeEdit:
      handleStartTimeEdit(event);
      break;
    case Screen::OverrideMenu:
      handleOverrideMenu(event);
      break;
    case Screen::OverrideEdit:
      handleOverrideEdit(event);
      break;
    case Screen::ResultView:
      handleResultView(event);
      break;
    case Screen::OrderAccessPrompt:
      handleOrderAccessPrompt(event);
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

void ApplicationCore::handleResultView(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  const size_t count = resultViewType_ == ResultViewType::StageResults
                           ? competition_.stageResults().size()
                           : (resultViewType_ == ResultViewType::Events
                                  ? eventRepository_.count()
                                  : 1);
  if (step && event.buttonId == ButtonId::Up && resultSelectedIndex_ > 0)
    --resultSelectedIndex_;
  else if (step && event.buttonId == ButtonId::Down &&
           resultSelectedIndex_ + 1 < count)
    ++resultSelectedIndex_;
  else if (event.eventType == ButtonEventType::Press &&
           event.buttonId == ButtonId::Left) {
    openSubmenu(MenuPage::Results);
    menuSelectedIndex_ = static_cast<uint8_t>(resultViewType_);
  }
}

void ApplicationCore::handleOverrideMenu(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  if (step && (event.buttonId == ButtonId::Up ||
               event.buttonId == ButtonId::Down)) {
    overrideMenuAction_ =
        overrideMenuAction_ == OverrideMenuAction::AdditionalOrder
            ? OverrideMenuAction::RoadBreak
            : OverrideMenuAction::AdditionalOrder;
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Left) {
    screen_ = Screen::BasicView;
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Right) {
    beginOverrideEdit();
  }
}

void ApplicationCore::beginOverrideEdit() {
  overrideStartSegmentIndex_ = competition_.currentSegmentIndex();
  overrideMaximumEndPointIndex_ =
      competition_.nextJatOrFinishPointIndex(overrideStartSegmentIndex_);
  overrideEndPointIndex_ = overrideStartSegmentIndex_ + 1;
  overrideDurationSeconds_ =
      overrideMenuAction_ == OverrideMenuAction::RoadBreak ? 660 : 60;
  overrideEditPhase_ =
      overrideMenuAction_ == OverrideMenuAction::RoadBreak
          ? OverrideEditPhase::Duration
          : OverrideEditPhase::StartSegment;
  overrideInvalid_ = false;
  screen_ = Screen::OverrideEdit;
}

void ApplicationCore::handleOverrideEdit(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  if (step && (event.buttonId == ButtonId::Up ||
               event.buttonId == ButtonId::Down)) {
    const bool up = event.buttonId == ButtonId::Up;
    if (overrideEditPhase_ == OverrideEditPhase::StartSegment) {
      const uint16_t current = competition_.currentSegmentIndex();
      const uint16_t lastStart = overrideMaximumEndPointIndex_ > 0
                                     ? overrideMaximumEndPointIndex_ - 1
                                     : current;
      if (up && overrideStartSegmentIndex_ < lastStart)
        ++overrideStartSegmentIndex_;
      else if (!up && overrideStartSegmentIndex_ > current)
        --overrideStartSegmentIndex_;
      overrideEndPointIndex_ = overrideStartSegmentIndex_ + 1;
      overrideMaximumEndPointIndex_ =
          competition_.nextJatOrFinishPointIndex(overrideStartSegmentIndex_);
    } else if (overrideEditPhase_ == OverrideEditPhase::Duration) {
      if (overrideMenuAction_ == OverrideMenuAction::RoadBreak) {
        static const uint16_t DURATIONS[] = {660, 1260, 1860};
        uint8_t selected = overrideDurationSeconds_ == 660
                               ? 0
                               : (overrideDurationSeconds_ == 1260 ? 1 : 2);
        selected = static_cast<uint8_t>((selected + (up ? 1 : 2)) % 3);
        overrideDurationSeconds_ = DURATIONS[selected];
      } else if (up && overrideDurationSeconds_ < 3599) {
        ++overrideDurationSeconds_;
      } else if (!up && overrideDurationSeconds_ > 1) {
        --overrideDurationSeconds_;
      }
    } else if (overrideEditPhase_ == OverrideEditPhase::EndPoint) {
      if (up && overrideEndPointIndex_ < overrideMaximumEndPointIndex_)
        ++overrideEndPointIndex_;
      else if (!up &&
               overrideEndPointIndex_ > overrideStartSegmentIndex_ + 1)
        --overrideEndPointIndex_;
    }
    overrideInvalid_ = false;
    return;
  }
  if (event.eventType != ButtonEventType::Press) return;
  if (event.buttonId == ButtonId::Left) {
    if (overrideEditPhase_ == OverrideEditPhase::EndPoint)
      overrideEditPhase_ = OverrideEditPhase::Duration;
    else if (overrideEditPhase_ == OverrideEditPhase::Duration &&
             overrideMenuAction_ == OverrideMenuAction::AdditionalOrder)
      overrideEditPhase_ = OverrideEditPhase::StartSegment;
    else
      screen_ = Screen::OverrideMenu;
  } else if (event.buttonId == ButtonId::Right) {
    if (overrideEditPhase_ == OverrideEditPhase::StartSegment)
      overrideEditPhase_ = OverrideEditPhase::Duration;
    else if (overrideEditPhase_ == OverrideEditPhase::Duration)
      overrideEditPhase_ = OverrideEditPhase::EndPoint;
    else
      acceptOverride();
  }
}

void ApplicationCore::acceptOverride() {
  domain::SegmentOverride requested;
  requested.overrideType =
      overrideMenuAction_ == OverrideMenuAction::AdditionalOrder
          ? domain::OverrideType::ADDITIONAL_ORDER
          : domain::OverrideType::ROAD_BREAK;
  requested.startSegmentIndex = overrideStartSegmentIndex_;
  requested.endPointIndex = overrideEndPointIndex_;
  requested.replacementDurationSeconds = overrideDurationSeconds_;
  requested.acceptedClockTime =
      eventClock(clock_.isSet() ? clock_.now() : ClockTime{0, 0, 0});
  domain::SegmentOverride accepted;
  if (!competition_.applyOverride(requested, accepted)) {
    overrideInvalid_ = true;
    return;
  }
  domain::EventRecord record = makeEvent(
      requested.overrideType == domain::OverrideType::ADDITIONAL_ORDER
          ? domain::DomainEventType::ADDITIONAL_ORDER
          : domain::DomainEventType::ROAD_BREAK);
  record.payload.hasOverride = true;
  record.payload.segmentOverride = accepted;
  appendEvent(record);
  screen_ = Screen::BasicView;
}

void ApplicationCore::handleJatResult(const ButtonEvent& event) {
  if (event.eventType == ButtonEventType::Press &&
      event.buttonId == ButtonId::Right) {
    competition_.dismissJatResult(clock_.elapsedSinceSetMilliseconds());
    beginJatStartTimeEdit();
  }
}

void ApplicationCore::beginJatStartTimeEdit() {
  startTimeCorrection_ = false;
  editedStageStartTime_ = competition_.proposedStartClockTime();
  originalStageStartProposal_ = editedStageStartTime_;
  emitOffsetAdjustmentMinutes_ = 0;
  stageStartProposalExpired_ = false;
  domain::EventRecord proposal =
      makeEvent(domain::DomainEventType::START_TIME_PROPOSED);
  proposal.payload.proposedStartClockTime = editedStageStartTime_;
  appendEvent(proposal);
  screen_ = Screen::StartTimeEdit;
}

void ApplicationCore::handleStartTimeEdit(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  if (step && (event.buttonId == ButtonId::Up ||
               event.buttonId == ButtonId::Down)) {
    const int8_t direction = event.buttonId == ButtonId::Up ? 1 : -1;
    if (competition_.pendingJatType() == domain::JatType::EMIT_JAT_OFFSET) {
      const int8_t candidate = emitOffsetAdjustmentMinutes_ + direction;
      if (candidate >= -2 && candidate <= 2) {
        emitOffsetAdjustmentMinutes_ = candidate;
        editedStageStartTime_ =
            addClockMinutes(originalStageStartProposal_, candidate);
      }
    } else {
      editedStageStartTime_ = addClockMinutes(editedStageStartTime_, direction);
    }
    stageStartProposalExpired_ = false;
    return;
  }
  if (event.eventType != ButtonEventType::Press) return;
  if (event.buttonId == ButtonId::Left) {
    if (startTimeCorrection_) {
      competition_.rejectStartTimeCorrection(
          clock_.elapsedSinceSetMilliseconds());
      startTimeCorrection_ = false;
      screen_ = Screen::BasicView;
      return;
    }
    editedStageStartTime_ = originalStageStartProposal_;
    emitOffsetAdjustmentMinutes_ = 0;
    stageStartProposalExpired_ = false;
  } else if (event.buttonId == ButtonId::Right &&
             (startTimeCorrection_ ||
              competition_.pendingJatType() !=
                  domain::JatType::EMIT_JAT_OFFSET)) {
    acceptJatStartTime();
  }
}

void ApplicationCore::acceptJatStartTime() {
  if (!clock_.isSet()) return;
  const bool correction = startTimeCorrection_;
  const domain::JatType jatType = competition_.pendingJatType();
  const ClockTime now = clock_.now();
  if (!competition_.acceptNextStageStart(
          editedStageStartTime_, millisecondsOfDay(now),
          clock_.elapsedSinceSetMilliseconds()))
    return;
  domain::EventRecord accepted = makeEvent(
      startTimeCorrection_ ? domain::DomainEventType::START_TIME_CORRECTED
                           : domain::DomainEventType::START_TIME_ACCEPTED);
  accepted.payload.acceptedStartClockTime = editedStageStartTime_;
  appendEvent(accepted);
  if (!correction && (jatType == domain::JatType::EMIT_MLA ||
                      jatType == domain::JatType::EMIT_ULA))
    resetTrip1();
  startTimeCorrection_ = false;
  screen_ = Screen::BasicView;
}

void ApplicationCore::handleMittisProposal(const ButtonEvent& event) {
  if (event.eventType != ButtonEventType::Press) return;
  if (event.buttonId == ButtonId::Left) {
    appendEvent(makeEvent(domain::DomainEventType::MITTIS_REJECTED));
    const uint16_t endedSegment = competition_.currentSegmentIndex();
    const domain::PointResult result = competition_.resolveMittisProposal();
    calibrationSaveFailed_ = false;
    completeMittisPoint(result, endedSegment);
  } else if (event.buttonId == ButtonId::Right &&
             competition_.mittisProposal().valid &&
             !calibrationSaveInFlight_) {
    editedMillimetersPerPulse_ =
        competition_.mittisProposal().proposedMillimetersPerPulse;
    mittisCalibrationSave_ = true;
    calibrationSavePending_ = true;
    calibrationSaveFailed_ = false;
  }
}

void ApplicationCore::completeMittisPoint(domain::PointResult result,
                                          uint16_t endedSegment) {
  if (result == domain::PointResult::ADVANCED) {
    domain::EventRecord point =
        makeEvent(domain::DomainEventType::NORMAL_POINT);
    point.segmentIndex = endedSegment;
    lastPointEventId_ = appendEvent(point);
    screen_ = Screen::BasicView;
    return;
  }
  if (result == domain::PointResult::FINISHED ||
      result == domain::PointResult::JAT_COMPLETED) {
    const bool finish = result == domain::PointResult::FINISHED;
    domain::EventRecord record = makeEvent(
        finish ? domain::DomainEventType::FINISH : domain::DomainEventType::JAT);
    record.segmentIndex = endedSegment;
    record.payload.finalDeltaMs = competition_.finalDeltaMs();
    const std::vector<domain::StageResult>& results =
        competition_.stageResults();
    if (!results.empty()) {
      record.stageIndex = results.back().stageIndex;
      record.payload.hasStageResult = true;
      record.payload.stageResult = results.back();
    }
    if (!finish) {
      record.payload.arrivalClockTime = competition_.arrivalClockTime();
      record.payload.hasJatType = true;
      record.payload.jatType = competition_.pendingJatType();
    }
    appendEvent(record);
    if (finish) {
      routeOrderCompletionPending_ = true;
      screen_ = Screen::BasicView;
    } else {
      resetTrip1();
      if (competition_.state() ==
          domain::CompetitionState::EDIT_START_TIME)
        beginJatStartTimeEdit();
      else
        screen_ = Screen::JatResult;
    }
    return;
  }
  screen_ = Screen::BasicView;
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
        activateCurrentRouteOrder();
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
  } else if (event.buttonId == ButtonId::Right &&
             competition_.beginStartTimeCorrection()) {
    editedStageStartTime_ = competition_.proposedStartClockTime();
    originalStageStartProposal_ = editedStageStartTime_;
    emitOffsetAdjustmentMinutes_ = 0;
    stageStartProposalExpired_ = false;
    startTimeCorrection_ = true;
    screen_ = Screen::StartTimeEdit;
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

void ApplicationCore::handleOrderAccessPrompt(const ButtonEvent& event) {
  const bool step = event.eventType == ButtonEventType::Press ||
                    event.eventType == ButtonEventType::LongRepeat;
  if (step && (event.buttonId == ButtonId::Up ||
               event.buttonId == ButtonId::Down)) {
    orderAccessAction_ = orderAccessAction_ == OrderAccessAction::Edit
                             ? OrderAccessAction::Replace
                             : OrderAccessAction::Edit;
    return;
  }
  if (event.eventType != ButtonEventType::Press) return;
  if (event.buttonId == ButtonId::Left) {
    openMainMenu(0);
  } else if (event.buttonId == ButtonId::Right) {
    if (orderAccessAction_ == OrderAccessAction::Edit)
      routeOrderEditor_.beginBrowse(currentRouteOrder_);
    else
      routeOrderEditor_.beginCreate();
    screen_ = Screen::OrderEdit;
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
      if (competition_.state() == domain::CompetitionState::WAIT_START ||
          competition_.state() == domain::CompetitionState::RUNNING) {
        orderAccessAction_ = OrderAccessAction::Edit;
        screen_ = Screen::OrderAccessPrompt;
        return;
      }
      if (competition_.state() != domain::CompetitionState::IDLE) return;
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
  } else if (menuPage_ == MenuPage::Results) {
    resultViewType_ = static_cast<ResultViewType>(menuSelectedIndex_);
    resultSelectedIndex_ = 0;
    screen_ = Screen::ResultView;
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
  domain::EventRecord record = makeEvent(domain::DomainEventType::TRIP_RESET);
  record.payload.tripChannel = domain::TripChannel::TRIP_1;
  appendEvent(record);
}

void ApplicationCore::resetTrip2() {
  trip2DistanceMm_ = 0;
  trip2PulseCount_ = 0;
  domain::EventRecord record = makeEvent(domain::DomainEventType::TRIP_RESET);
  record.payload.tripChannel = domain::TripChannel::TRIP_2;
  appendEvent(record);
}

void ApplicationCore::handleDistancePulses(const DistancePulseEvent& event) {
  const uint64_t unsignedDistance =
      domain::motion::distanceMillimetersForPulses(event.pulseCount,
                                                   millimetersPerPulse_);
  const int64_t magnitude =
      unsignedDistance > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
          ? std::numeric_limits<int64_t>::max()
          : static_cast<int64_t>(unsignedDistance);
  if (event.reverseActive != reverseActive_) {
    handleReverseSignal({event.reverseActive,
                         static_cast<uint32_t>(event.observedAtUs / 1000UL)});
  }
  const int64_t distanceDelta = event.reverseActive ? -magnitude : magnitude;
  totalPulseCount_ =
      domain::motion::saturatingAdd(totalPulseCount_, event.pulseCount);
  addDistance(distanceDelta, event.pulseCount);
  if (atOverlayVisible_ && atDistanceArmed_) {
    atTravelledDistanceMm_ = domain::motion::saturatingAddSigned(
        atTravelledDistanceMm_, signedMagnitude(distanceDelta));
    const int64_t target =
        static_cast<int64_t>(competitionSettings_.atDisplayDistanceM) * 1000LL;
    if (atTravelledDistanceMm_ >= target) atOverlayVisible_ = false;
  }
  competition_.addDistanceMillimeters(distanceDelta);
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

void ApplicationCore::handleReverseSignal(const ReverseSignalEvent& event) {
  if (reverseActive_ == event.reverseActive) return;
  reverseActive_ = event.reverseActive;
  competition_.setReverseActive(reverseActive_);
  domain::EventRecord record = makeEvent(domain::DomainEventType::REVERSE_CHANGED);
  record.monotonicTimeMs = event.monotonicMs;
  record.payload.reverseActive = reverseActive_;
  appendEvent(record);
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
    const domain::CompetitionState before = competition_.state();
    competition_.tick(clock_.elapsedSinceSetMilliseconds());
    if (before == domain::CompetitionState::JAT_RESULT &&
        competition_.state() == domain::CompetitionState::EDIT_START_TIME)
      beginJatStartTimeEdit();
    if (screen_ == Screen::StartTimeEdit) {
      const ClockTime nowClock = clock_.now();
      const int32_t nowMinutes = nowClock.hour * 60 + nowClock.minute;
      const int32_t proposedMinutes =
          editedStageStartTime_.hour * 60 + editedStageStartTime_.minute;
      int32_t difference = proposedMinutes - nowMinutes;
      if (difference > 720) difference -= 1440;
      if (difference < -720) difference += 1440;
      if (difference < 0 ||
          (difference == 0 && nowClock.second >= editedStageStartTime_.second))
        stageStartProposalExpired_ = true;
    }
    if (atOverlayVisible_ && !atDistanceArmed_) {
      const uint64_t nowMs = clock_.elapsedSinceSetMilliseconds();
      if (speedCalculator_.speedKmh() <= 3.6F) {
        if (!atLowSpeedTiming_) {
          atLowSpeedTiming_ = true;
          atLowSpeedSinceMs_ = nowMs;
        } else if (nowMs - atLowSpeedSinceMs_ >= 1000) {
          atDistanceArmed_ = true;
          atTravelledDistanceMm_ = 0;
        }
      } else {
        atLowSpeedTiming_ = false;
      }
    }
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
  const domain::MittisProposal& mittis = competition_.mittisProposal();
  model.mittis = {mittis.oldMillimetersPerPulse,
                  mittis.proposedMillimetersPerPulse,
                  mittis.measuredDistanceMillimeters,
                  mittis.referenceDistanceMeters,
                  mittis.valid,
                  calibrationSaveFailed_};
  model.atOverlay = {atOverlayVisible_, atClockTime_, !atDistanceArmed_,
                     atTravelledDistanceMm_,
                     competitionSettings_.atDisplayDistanceM};
  model.jatResult = {competition_.arrivalClockTime(),
                     competition_.finalDeltaMs() / 1000};
  model.finishResult.visible =
      competition_.state() == domain::CompetitionState::FINISHED &&
      !competition_.stageResults().empty();
  model.finishResult.totalPoints = competition_.totalPoints();
  if (model.finishResult.visible)
    model.finishResult.finishClockTime =
        competition_.stageResults().back().endClockTime;
  const uint64_t elapsed =
      clock_.isSet() ? clock_.elapsedSinceSetMilliseconds() : 0;
  model.startTimeEdit = {
      editedStageStartTime_, competition_.pendingJatType(),
      !stageStartProposalExpired_ || ((elapsed / 500ULL) % 2ULL == 0),
      !startTimeCorrection_ &&
          competition_.pendingJatType() == domain::JatType::EMIT_JAT_OFFSET};
  model.overrideMenu = {overrideMenuAction_};
  model.overrideEdit = {overrideMenuAction_, overrideEditPhase_,
                        overrideStartSegmentIndex_, overrideEndPointIndex_,
                        overrideMaximumEndPointIndex_,
                        overrideDurationSeconds_, overrideInvalid_};
  model.resultView.type = resultViewType_;
  model.resultView.selectedIndex = resultSelectedIndex_;
  model.resultView.itemCount = 0;
  model.resultView.totalPoints = competition_.totalPoints();
  if (resultViewType_ == ResultViewType::StageResults) {
    const std::vector<domain::StageResult>& results =
        competition_.stageResults();
    model.resultView.itemCount = static_cast<uint16_t>(
        results.size() > 0xFFFFU ? 0xFFFFU : results.size());
    if (resultSelectedIndex_ < results.size()) {
      model.resultView.hasStageResult = true;
      model.resultView.stageResult = results[resultSelectedIndex_];
    }
  } else if (resultViewType_ == ResultViewType::TotalPoints) {
    model.resultView.itemCount = 1;
  } else {
    model.resultView.itemCount = static_cast<uint16_t>(
        eventRepository_.count() > 0xFFFFU ? 0xFFFFU
                                           : eventRepository_.count());
    const domain::EventRecord* event =
        eventRepository_.at(resultSelectedIndex_);
    if (event) {
      model.resultView.hasEvent = true;
      model.resultView.event = *event;
    }
  }
  model.order.editor = routeOrderEditor_.view();
  model.orderAccess.selectedAction = orderAccessAction_;
  model.order.hasSelectedSegment = false;
  model.order.showsStartTime = false;
  model.competition.state = competition_.state();
  model.competition.deltaSeconds = competition_.deltaSeconds();
  model.competition.deltaFrozen = competition_.deltaFrozen();
  model.competition.undoPromptVisible = competition_.hasUndoPrompt(
      clock_.isSet() ? clock_.elapsedSinceSetMilliseconds() : 0);
  const domain::SegmentDefinition* current = competition_.currentSegment();
  if (current) {
    model.competition.currentSegment = *current;
    model.competition.hasCurrentSegment = true;
  }
  const domain::SegmentDefinition* next = competition_.nextSegment();
  if (next) {
    model.competition.nextSegment = *next;
    model.competition.hasNextSegment = true;
  }
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
    for (uint8_t index = 0; index < 8; ++index) {
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
    model.diagnostics.lastPointEventType = lastPointEventType_;
    model.diagnostics.lastAtEventType = lastAtEventType_;
    model.diagnostics.reverseActive = reverseActive_;
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
  if (mittisCalibrationSave_) {
    mittisCalibrationSave_ = false;
    if (succeeded) {
      millimetersPerPulse_ = editedMillimetersPerPulse_;
      speedCalculator_.setMillimetersPerPulse(millimetersPerPulse_);
      domain::EventRecord accepted =
          makeEvent(domain::DomainEventType::MITTIS_ACCEPTED);
      accepted.payload.oldCalibration =
          competition_.mittisProposal().oldMillimetersPerPulse;
      accepted.payload.proposedCalibration = millimetersPerPulse_;
      appendEvent(accepted);
      const uint16_t endedSegment = competition_.currentSegmentIndex();
      const domain::PointResult result = competition_.resolveMittisProposal();
      calibrationSaveFailed_ = false;
      completeMittisPoint(result, endedSegment);
    } else {
      editedMillimetersPerPulse_ = millimetersPerPulse_;
      calibrationSaveFailed_ = true;
    }
    return;
  }
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

void ApplicationCore::setCompetitionSettings(
    const domain::CompetitionSettings& settings) {
  competitionSettings_ = domain::validatedCompetitionSettings(settings);
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

void ApplicationCore::setInitialRouteOrder(const domain::RouteOrder& order,
                                           bool active) {
  if (domain::validateRouteOrder(order) ==
      domain::RouteOrderValidationError::NONE) {
    currentRouteOrder_ = order;
    hasRouteOrder_ = true;
    loadedRouteOrderActive_ = active;
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
    loadedRouteOrderActive_ = true;
    activateCurrentRouteOrder();
    screen_ = Screen::BasicView;
  }
  routeOrderEditor_.completeSave(succeeded);
}

void ApplicationCore::activateCurrentRouteOrder() {
  if (!hasRouteOrder_ || !loadedRouteOrderActive_ || !clock_.isSet()) return;
  const ClockTime now = clock_.now();
  const uint32_t millisecondsOfDay =
      (static_cast<uint32_t>(now.hour) * 3600UL +
       static_cast<uint32_t>(now.minute) * 60UL + now.second) *
      1000UL;
  if (competition_.activate(currentRouteOrder_, millisecondsOfDay,
                            clock_.elapsedSinceSetMilliseconds())) {
    domain::EventRecord accepted =
        makeEvent(domain::DomainEventType::START_TIME_ACCEPTED);
    accepted.payload.acceptedStartClockTime =
        {currentRouteOrder_.startHour, currentRouteOrder_.startMinute, 0};
    appendEvent(accepted);
    loadedRouteOrderActive_ = false;
  }
}

domain::EventRecord ApplicationCore::makeEvent(
    domain::DomainEventType type) const {
  domain::EventRecord record;
  record.eventType = type;
  record.clockTime = eventClock(clock_.isSet() ? clock_.now() : ClockTime{0, 0, 0});
  record.monotonicTimeMs =
      clock_.isSet() ? clock_.elapsedSinceSetMilliseconds() : 0;
  record.segmentIndex = competition_.currentSegmentIndex();
  record.competitionTimeMs = competition_.realTimeMs();
  record.tIdealMs = competition_.idealTimeMs();
  record.deltaMs = competition_.deltaMs();
  record.physicalDistanceMillimeters =
      competition_.physicalDistanceMillimeters();
  record.stageDistanceMillimeters = competition_.stageDistanceMillimeters();
  record.segmentDistanceMillimeters =
      competition_.segmentDistanceMillimeters();
  record.trip1DistanceMillimeters = trip1DistanceMm_;
  record.trip2DistanceMillimeters = trip2DistanceMm_;
  const float speedMilli = speedCalculator_.speedKmh() * 1000.0F;
  record.speedKmhMilli =
      speedMilli >= static_cast<float>(std::numeric_limits<int32_t>::max())
          ? std::numeric_limits<int32_t>::max()
          : static_cast<int32_t>(speedMilli);
  record.reverseActive = reverseActive_;
  return record;
}

uint64_t ApplicationCore::appendEvent(domain::EventRecord record) {
  uint64_t eventId = 0;
  return eventRepository_.append(record, eventId) ? eventId : 0;
}

void ApplicationCore::handleAtRelease() {
  const uint64_t nowMs =
      clock_.isSet() ? clock_.elapsedSinceSetMilliseconds() : 0;
  if (atEventCancellable_ && nowMs - atEventMonotonicMs_ <= 3000) {
    eventRepository_.markCancelled(atEventId_);
    domain::EventRecord cancellation =
        makeEvent(domain::DomainEventType::AT_CANCELLED);
    cancellation.payload.referencedEventId = atEventId_;
    appendEvent(cancellation);
    atEventCancellable_ = false;
    atOverlayVisible_ = false;
    return;
  }
  domain::EventRecord record = makeEvent(domain::DomainEventType::AT);
  atEventId_ = appendEvent(record);
  atEventCancellable_ = atEventId_ != 0;
  atEventMonotonicMs_ = nowMs;
  atClockTime_ = record.clockTime;
  atOverlayVisible_ = true;
  atLowSpeedTiming_ = false;
  atDistanceArmed_ = false;
  atTravelledDistanceMm_ = 0;
}

bool ApplicationCore::takeRouteOrderCompletionRequest() {
  if (!routeOrderCompletionPending_ || routeOrderCompletionInFlight_) return false;
  routeOrderCompletionPending_ = false;
  routeOrderCompletionInFlight_ = true;
  return true;
}

void ApplicationCore::completeRouteOrderCompletion(bool succeeded) {
  if (!routeOrderCompletionInFlight_) return;
  routeOrderCompletionInFlight_ = false;
  if (!succeeded) routeOrderCompletionPending_ = true;
}

void ApplicationCore::addDistance(int64_t deltaMillimeters,
                                  uint32_t pulseCount) {
  if (!trip1ResetHeld_) {
    trip1DistanceMm_ = domain::motion::saturatingAddSigned(
        trip1DistanceMm_, deltaMillimeters);
    trip1PulseCount_ =
        domain::motion::saturatingAdd(trip1PulseCount_, pulseCount);
  }
  trip2DistanceMm_ = domain::motion::saturatingAddSigned(
      trip2DistanceMm_, deltaMillimeters);
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
