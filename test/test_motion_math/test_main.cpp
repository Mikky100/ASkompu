#include <cstdint>
#include <limits>

#include <unity.h>

#include "CalibrationConfig.h"
#include "domain/CalibrationSetting.h"
#include "domain/MotionMath.h"
#include "domain/SpeedCalculator.h"
#include "domain/TripCounter.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr uint32_t ZERO_SPEED_TIMEOUT_US = 1000000;

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
  return UNITY_END();
}
