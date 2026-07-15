#include <cstdint>
#include <limits>

#include <unity.h>

#include "CalibrationConfig.h"
#include "core/ApplicationCore.h"
#include "domain/CalibrationSetting.h"
#include "domain/MotionMath.h"
#include "domain/SpeedCalculator.h"
#include "domain/TripCounter.h"
#include "input/ButtonInterpreter.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr uint32_t ZERO_SPEED_TIMEOUT_US = 1000000;

core::ButtonEvent button(core::ButtonId id, core::ButtonEventType type,
                         uint32_t nowMs = 0) {
  return {id, type, nowMs};
}

void pulse(core::ApplicationCore& application, uint32_t count,
           uint32_t previousAtUs, uint32_t lastAtUs,
           uint32_t observedAtUs) {
  application.handleDistancePulses(
      {count, previousAtUs, lastAtUs, observedAtUs});
}

void assertSpeed(uint32_t millimetersPerPulse, uint32_t intervalUs,
                 float expectedKmh) {
  domain::SpeedCalculator calculator(millimetersPerPulse,
                                     ZERO_SPEED_TIMEOUT_US);
  calculator.update(intervalUs * 2, 2, intervalUs, intervalUs * 2);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, expectedKmh, calculator.speedKmh());
}

void testSparsePulseSpeeds() {
  assertSpeed(1000, 100000, 36.0F);
  assertSpeed(1000, 60000, 60.0F);
  assertSpeed(1000, 30000, 120.0F);
}

void testDensePulseSpeeds() {
  assertSpeed(100, 10000, 36.0F);
  assertSpeed(100, 6000, 60.0F);
  assertSpeed(100, 3000, 120.0F);
}

void testCalibrationValidationAndFallback() {
  TEST_ASSERT_FALSE(domain::calibration::isValid(0));
  TEST_ASSERT_TRUE(domain::calibration::isValid(
      CalibrationConfig::MIN_MILLIMETERS_PER_PULSE));
  TEST_ASSERT_TRUE(domain::calibration::isValid(
      CalibrationConfig::MAX_MILLIMETERS_PER_PULSE));
  TEST_ASSERT_FALSE(domain::calibration::isValid(
      CalibrationConfig::MAX_MILLIMETERS_PER_PULSE + 1));
  TEST_ASSERT_FALSE(domain::calibration::isValid(UINT32_MAX));
  TEST_ASSERT_EQUAL_UINT32(
      CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE,
      domain::calibration::validatedOrDefault(0));
}

void testCalibrationStepAndLimits() {
  TEST_ASSERT_EQUAL_UINT32(1001, domain::calibration::increment(1000));
  TEST_ASSERT_EQUAL_UINT32(999, domain::calibration::decrement(1000));
  TEST_ASSERT_EQUAL_UINT32(
      CalibrationConfig::MAX_MILLIMETERS_PER_PULSE,
      domain::calibration::increment(
          CalibrationConfig::MAX_MILLIMETERS_PER_PULSE));
  TEST_ASSERT_EQUAL_UINT32(
      CalibrationConfig::MIN_MILLIMETERS_PER_PULSE,
      domain::calibration::decrement(
          CalibrationConfig::MIN_MILLIMETERS_PER_PULSE));
}

void testCalibrationChangeAffectsOnlyFuturePulses() {
  domain::TripCounter trip(1000);
  trip.addPulses(1);
  trip.setMillimetersPerPulse(100);
  trip.addPulses(1);

  TEST_ASSERT_EQUAL_UINT64(1100, trip.distanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(2, trip.pulseCount());

  domain::SpeedCalculator calculator(1000, ZERO_SPEED_TIMEOUT_US);
  calculator.update(200000, 2, 100000, 200000);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, calculator.speedKmh());
  calculator.setMillimetersPerPulse(100);
  calculator.update(210000, 3, 200000, 210000);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, calculator.speedKmh());
}

void testThirtySecondStageDistances() {
  TEST_ASSERT_EQUAL_UINT64(
      300000, domain::motion::distanceMillimetersForPulses(300, 1000));
  TEST_ASSERT_EQUAL_UINT64(
      500000, domain::motion::distanceMillimetersForPulses(500, 1000));
  TEST_ASSERT_EQUAL_UINT64(
      1000000, domain::motion::distanceMillimetersForPulses(1000, 1000));
}

void testGeneratorCycleDistanceAndPauses() {
  domain::TripCounter trip(1000);

  trip.addPulses(300);
  TEST_ASSERT_EQUAL_UINT64(300000, trip.distanceMillimeters());
  trip.addPulses(0);
  TEST_ASSERT_EQUAL_UINT64(300000, trip.distanceMillimeters());

  trip.addPulses(500);
  TEST_ASSERT_EQUAL_UINT64(800000, trip.distanceMillimeters());
  trip.addPulses(0);

  trip.addPulses(1000);
  TEST_ASSERT_EQUAL_UINT64(1800000, trip.distanceMillimeters());
  trip.addPulses(0);
  TEST_ASSERT_EQUAL_UINT64(1800000, trip.distanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(1800, trip.pulseCount());
}

void testTripResetDoesNotChangeTotalPulses() {
  uint32_t totalPulses = 300;
  domain::TripCounter trip(1000);
  trip.addPulses(totalPulses);

  trip.reset();

  TEST_ASSERT_EQUAL_UINT32(300, totalPulses);
  TEST_ASSERT_EQUAL_UINT64(0, trip.pulseCount());
  TEST_ASSERT_EQUAL_UINT64(0, trip.distanceMillimeters());
}

void testSpeedReturnsToZeroAtTimeout() {
  domain::SpeedCalculator calculator(1000, ZERO_SPEED_TIMEOUT_US);
  calculator.update(200000, 2, 100000, 200000);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, calculator.speedKmh());

  calculator.update(1199999, 2, 100000, 200000);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, calculator.speedKmh());
  calculator.update(1200000, 2, 100000, 200000);
  TEST_ASSERT_EQUAL_FLOAT(0.0F, calculator.speedKmh());
}

void testTimestampRollover() {
  domain::SpeedCalculator calculator(1000, ZERO_SPEED_TIMEOUT_US);
  constexpr uint32_t previousPulseAtUs = UINT32_MAX - 49999U;
  constexpr uint32_t lastPulseAtUs = 50000U;

  calculator.update(lastPulseAtUs, 2, previousPulseAtUs, lastPulseAtUs);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, calculator.speedKmh());

  calculator.update(lastPulseAtUs + ZERO_SPEED_TIMEOUT_US, 2,
                    previousPulseAtUs, lastPulseAtUs);
  TEST_ASSERT_EQUAL_FLOAT(0.0F, calculator.speedKmh());
}

void testDistanceMultiplicationAndAdditionSaturateSafely() {
  const uint64_t maximumProduct =
      domain::motion::distanceMillimetersForPulses(UINT32_MAX, UINT32_MAX);
  TEST_ASSERT_EQUAL_UINT64(static_cast<uint64_t>(UINT32_MAX) * UINT32_MAX,
                           maximumProduct);
  TEST_ASSERT_EQUAL_UINT64(
      std::numeric_limits<uint64_t>::max(),
      domain::motion::saturatingAdd(
          std::numeric_limits<uint64_t>::max() - 5, 6));
}

void testApplicationPulseUpdatesBothTripsAndDisplayModel() {
  core::ApplicationCore application(1000, ZERO_SPEED_TIMEOUT_US);
  pulse(application, 2, 100000, 200000, 200000);

  TEST_ASSERT_EQUAL_UINT64(2000, application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(2000, application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(2, application.totalPulseCount());

  const core::DisplayModel model = application.displayModel();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Drive),
                          static_cast<uint8_t>(model.screen));
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, model.speedKmh);
  TEST_ASSERT_EQUAL_UINT64(2000, model.trip1.distanceMillimeters);
  TEST_ASSERT_EQUAL_UINT64(2000, model.trip2.distanceMillimeters);
  TEST_ASSERT_TRUE(model.trip2.visible);
}

void testTripResetsAreIndependentAndKeepDiagnosticPulseCount() {
  core::ApplicationCore application(1000, ZERO_SPEED_TIMEOUT_US);
  pulse(application, 10, 900000, 1000000, 1000000);

  application.handleButton(
      button(core::ButtonId::Trip1Reset, core::ButtonEventType::Press));
  TEST_ASSERT_EQUAL_UINT64(0, application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(10000, application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(10, application.totalPulseCount());
  TEST_ASSERT_EQUAL_UINT64(
      0, application.displayModel().trip1.distanceMillimeters);

  pulse(application, 2, 1000000, 1100000, 1100000);
  TEST_ASSERT_EQUAL_UINT64(0, application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(12000, application.trip2DistanceMillimeters());
  application.handleButton(
      button(core::ButtonId::Trip1Reset, core::ButtonEventType::Release));
  pulse(application, 1, 1100000, 1200000, 1200000);

  application.handleButton(
      button(core::ButtonId::Trip2Reset, core::ButtonEventType::Press));
  TEST_ASSERT_EQUAL_UINT64(1000, application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(0, application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(13, application.totalPulseCount());
  TEST_ASSERT_EQUAL_UINT64(
      0, application.displayModel().trip2.distanceMillimeters);
}

void testApplicationGeneratorCycleAndZeroTimeout() {
  core::ApplicationCore application(1000, ZERO_SPEED_TIMEOUT_US);
  pulse(application, 300, 29900000, 30000000, 30000000);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, application.displayModel().speedKmh);
  application.tick(40000000);
  TEST_ASSERT_EQUAL_FLOAT(0.0F, application.displayModel().speedKmh);
  TEST_ASSERT_EQUAL_UINT64(300000, application.trip1DistanceMillimeters());

  pulse(application, 500, 69940000, 70000000, 70000000);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 60.0F, application.displayModel().speedKmh);
  application.tick(80000000);
  TEST_ASSERT_EQUAL_UINT64(800000, application.trip1DistanceMillimeters());

  pulse(application, 1000, 109970000, 110000000, 110000000);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 120.0F,
                           application.displayModel().speedKmh);
  application.tick(120000000);
  TEST_ASSERT_EQUAL_UINT64(1800000,
                           application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(1800000,
                           application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(1800, application.totalPulseCount());
  TEST_ASSERT_EQUAL_FLOAT(0.0F, application.displayModel().speedKmh);
}

void testApplicationDenseCalibrationDistanceAndSpeed() {
  core::ApplicationCore application(100, ZERO_SPEED_TIMEOUT_US);
  pulse(application, 100, 990000, 1000000, 1000000);
  TEST_ASSERT_EQUAL_UINT64(10000, application.trip1DistanceMillimeters());
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F,
                           application.displayModel().speedKmh);
}

void testCalibrationCancelAcceptAndUnchangedWriteAvoidance() {
  core::ApplicationCore application(1000, ZERO_SPEED_TIMEOUT_US);
  uint32_t requestedValue = 0;

  application.handleButton(button(core::ButtonId::Up,
                                  core::ButtonEventType::Press));
  TEST_ASSERT_EQUAL_UINT32(
      1001, application.displayModel().calibration.editedMillimetersPerPulse);
  application.handleButton(button(core::ButtonId::Left,
                                  core::ButtonEventType::Press));
  TEST_ASSERT_FALSE(application.takeCalibrationSaveRequest(requestedValue));
  TEST_ASSERT_EQUAL_UINT32(1000, application.millimetersPerPulse());

  application.handleButton(button(core::ButtonId::Up,
                                  core::ButtonEventType::Press));
  application.handleButton(button(core::ButtonId::Down,
                                  core::ButtonEventType::Press));
  application.handleButton(button(core::ButtonId::Right,
                                  core::ButtonEventType::Press));
  TEST_ASSERT_FALSE(application.takeCalibrationSaveRequest(requestedValue));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Drive),
                          static_cast<uint8_t>(application.screen()));

  application.handleButton(button(core::ButtonId::Down,
                                  core::ButtonEventType::Press));
  application.handleButton(button(core::ButtonId::Right,
                                  core::ButtonEventType::Press));
  TEST_ASSERT_TRUE(application.takeCalibrationSaveRequest(requestedValue));
  TEST_ASSERT_EQUAL_UINT32(999, requestedValue);
  application.completeCalibrationSave(true);
  TEST_ASSERT_EQUAL_UINT32(999, application.millimetersPerPulse());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Drive),
                          static_cast<uint8_t>(application.screen()));
}

void testCalibrationSaveFailureStaysInEditor() {
  core::ApplicationCore application(1000, ZERO_SPEED_TIMEOUT_US);
  uint32_t requestedValue = 0;
  application.handleButton(button(core::ButtonId::Up,
                                  core::ButtonEventType::Press));
  application.handleButton(button(core::ButtonId::Right,
                                  core::ButtonEventType::Press));
  TEST_ASSERT_TRUE(application.takeCalibrationSaveRequest(requestedValue));
  application.completeCalibrationSave(false);
  const core::DisplayModel model = application.displayModel();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Calibration),
                          static_cast<uint8_t>(model.screen));
  TEST_ASSERT_TRUE(model.calibration.saveFailed);
  TEST_ASSERT_EQUAL_UINT32(1000, application.millimetersPerPulse());
}

void testApplicationValidatesInitialCalibration() {
  core::ApplicationCore missing(0, ZERO_SPEED_TIMEOUT_US);
  core::ApplicationCore tooLarge(UINT32_MAX, ZERO_SPEED_TIMEOUT_US);
  core::ApplicationCore accepted(1234, ZERO_SPEED_TIMEOUT_US);
  TEST_ASSERT_EQUAL_UINT32(
      CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE,
      missing.millimetersPerPulse());
  TEST_ASSERT_EQUAL_UINT32(
      CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE,
      tooLarge.millimetersPerPulse());
  TEST_ASSERT_EQUAL_UINT32(1234, accepted.millimetersPerPulse());
}

void testButtonDebounceProducesOneShortPress() {
  input::ButtonInterpreter button(35, 500, 100);
  button.reset(false, 0);
  TEST_ASSERT_FALSE(button.update(true, 10).pressed);
  TEST_ASSERT_FALSE(button.update(false, 20).pressed);
  TEST_ASSERT_FALSE(button.update(true, 25).pressed);
  const input::ButtonTransitions pressed = button.update(true, 60);
  TEST_ASSERT_TRUE(pressed.pressed);
  TEST_ASSERT_FALSE(button.update(false, 80).released);
  const input::ButtonTransitions released = button.update(false, 115);
  TEST_ASSERT_TRUE(released.released);
  TEST_ASSERT_TRUE(released.shortPress);
  TEST_ASSERT_FALSE(released.longStart);
}

void testLongButtonPressDoesNotProduceShortPress() {
  input::ButtonInterpreter button(35, 500, 100);
  button.reset(false, 0);
  button.update(true, 10);
  TEST_ASSERT_TRUE(button.update(true, 45).pressed);
  const input::ButtonTransitions longStart = button.update(true, 545);
  TEST_ASSERT_TRUE(longStart.longStart);
  TEST_ASSERT_FALSE(longStart.shortPress);
  TEST_ASSERT_TRUE(button.update(true, 645).longRepeat);
  button.update(false, 700);
  const input::ButtonTransitions released = button.update(false, 735);
  TEST_ASSERT_TRUE(released.released);
  TEST_ASSERT_FALSE(released.shortPress);
}

void testTripDistanceSaturatesDuringVeryLongRun() {
  core::ApplicationCore application(
      CalibrationConfig::MAX_MILLIMETERS_PER_PULSE, ZERO_SPEED_TIMEOUT_US);
  for (uint32_t index = 0; index < 43000; ++index) {
    pulse(application, UINT32_MAX, 1, 2, 2);
  }
  TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                           application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                           application.trip2DistanceMillimeters());
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testSparsePulseSpeeds);
  RUN_TEST(testDensePulseSpeeds);
  RUN_TEST(testCalibrationValidationAndFallback);
  RUN_TEST(testCalibrationStepAndLimits);
  RUN_TEST(testCalibrationChangeAffectsOnlyFuturePulses);
  RUN_TEST(testThirtySecondStageDistances);
  RUN_TEST(testGeneratorCycleDistanceAndPauses);
  RUN_TEST(testTripResetDoesNotChangeTotalPulses);
  RUN_TEST(testSpeedReturnsToZeroAtTimeout);
  RUN_TEST(testTimestampRollover);
  RUN_TEST(testDistanceMultiplicationAndAdditionSaturateSafely);
  RUN_TEST(testApplicationPulseUpdatesBothTripsAndDisplayModel);
  RUN_TEST(testTripResetsAreIndependentAndKeepDiagnosticPulseCount);
  RUN_TEST(testApplicationGeneratorCycleAndZeroTimeout);
  RUN_TEST(testApplicationDenseCalibrationDistanceAndSpeed);
  RUN_TEST(testCalibrationCancelAcceptAndUnchangedWriteAvoidance);
  RUN_TEST(testCalibrationSaveFailureStaysInEditor);
  RUN_TEST(testApplicationValidatesInitialCalibration);
  RUN_TEST(testButtonDebounceProducesOneShortPress);
  RUN_TEST(testLongButtonPressDoesNotProduceShortPress);
  RUN_TEST(testTripDistanceSaturatesDuringVeryLongRun);
  return UNITY_END();
}
