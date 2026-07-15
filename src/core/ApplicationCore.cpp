#include "ApplicationCore.h"

#include <limits>

#include "domain/CalibrationSetting.h"
#include "domain/MotionMath.h"

namespace core {

ApplicationCore::ApplicationCore(uint32_t millimetersPerPulse,
                                 uint32_t zeroSpeedTimeoutUs)
    : speedCalculator_(
          domain::calibration::validatedOrDefault(millimetersPerPulse),
          zeroSpeedTimeoutUs),
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
  if (event.buttonId == ButtonId::Trip1Reset ||
      event.buttonId == ButtonId::FootReset) {
    if (event.eventType == ButtonEventType::Press) {
      trip1ResetHeld_ = true;
      trip1DistanceMm_ = 0;
    } else if (event.eventType == ButtonEventType::Release) {
      trip1ResetHeld_ = false;
    }
    return;
  }

  if (event.buttonId == ButtonId::Trip2Reset &&
      event.eventType == ButtonEventType::Press) {
    trip2DistanceMm_ = 0;
    return;
  }

  const bool stepEvent = event.eventType == ButtonEventType::Press ||
                         event.eventType == ButtonEventType::LongRepeat;
  if (screen_ == Screen::Drive) {
    if (event.eventType == ButtonEventType::Press &&
        event.buttonId == ButtonId::Up) {
      openCalibration(true);
    } else if (event.eventType == ButtonEventType::Press &&
               event.buttonId == ButtonId::Down) {
      openCalibration(false);
    }
    return;
  }

  if (stepEvent && event.buttonId == ButtonId::Up) {
    editedMillimetersPerPulse_ =
        domain::calibration::increment(editedMillimetersPerPulse_);
    calibrationSaveFailed_ = false;
  } else if (stepEvent && event.buttonId == ButtonId::Down) {
    editedMillimetersPerPulse_ =
        domain::calibration::decrement(editedMillimetersPerPulse_);
    calibrationSaveFailed_ = false;
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Left) {
    screen_ = Screen::Drive;
    calibrationSaveFailed_ = false;
    calibrationSavePending_ = false;
  } else if (event.eventType == ButtonEventType::Press &&
             event.buttonId == ButtonId::Right &&
             !calibrationSaveInFlight_) {
    if (editedMillimetersPerPulse_ == millimetersPerPulse_) {
      screen_ = Screen::Drive;
    } else {
      calibrationSavePending_ = true;
      calibrationSaveFailed_ = false;
    }
  }
}

void ApplicationCore::handleDistancePulses(const DistancePulseEvent& event) {
  const uint64_t distanceIncrement =
      domain::motion::distanceMillimetersForPulses(event.pulseCount,
                                                   millimetersPerPulse_);
  totalPulseCount_ =
      domain::motion::saturatingAdd(totalPulseCount_, event.pulseCount);
  addDistance(distanceIncrement);

  if (event.pulseCount > 0) {
    previousPulseAtUs_ = event.previousPulseAtUs;
    lastPulseAtUs_ = event.lastPulseAtUs;
  }
  const uint32_t speedPulseCount =
      totalPulseCount_ > std::numeric_limits<uint32_t>::max()
          ? std::numeric_limits<uint32_t>::max()
          : static_cast<uint32_t>(totalPulseCount_);
  speedCalculator_.update(event.observedAtUs, speedPulseCount,
                          previousPulseAtUs_, lastPulseAtUs_);
}

void ApplicationCore::tick(uint32_t nowUs) {
  const uint32_t speedPulseCount =
      totalPulseCount_ > std::numeric_limits<uint32_t>::max()
          ? std::numeric_limits<uint32_t>::max()
          : static_cast<uint32_t>(totalPulseCount_);
  speedCalculator_.update(nowUs, speedPulseCount, previousPulseAtUs_,
                          lastPulseAtUs_);
}

DisplayModel ApplicationCore::displayModel() const {
  return {screen_,
          speedCalculator_.speedKmh(),
          {trip1DistanceMm_, true},
          {trip2DistanceMm_, true},
          totalPulseCount_,
          millimetersPerPulse_,
          {editedMillimetersPerPulse_, calibrationSaveFailed_}};
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
    screen_ = Screen::Drive;
    calibrationSaveFailed_ = false;
  } else {
    calibrationSaveFailed_ = true;
  }
}

void ApplicationCore::openCalibration(bool incrementValue) {
  editedMillimetersPerPulse_ =
      incrementValue ? domain::calibration::increment(millimetersPerPulse_)
                     : domain::calibration::decrement(millimetersPerPulse_);
  calibrationSaveFailed_ = false;
  calibrationSavePending_ = false;
  screen_ = Screen::Calibration;
}

void ApplicationCore::addDistance(uint64_t incrementMillimeters) {
  if (!trip1ResetHeld_) {
    trip1DistanceMm_ =
        domain::motion::saturatingAdd(trip1DistanceMm_, incrementMillimeters);
  }
  trip2DistanceMm_ =
      domain::motion::saturatingAdd(trip2DistanceMm_, incrementMillimeters);
}

}  // namespace core
