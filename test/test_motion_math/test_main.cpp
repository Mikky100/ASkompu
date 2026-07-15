#include <cstdint>
#include <cstring>
#include <limits>

#include <unity.h>

#include "CalibrationConfig.h"
#include "core/ApplicationCore.h"
#include "core/Clock.h"
#include "domain/CalibrationSetting.h"
#include "domain/MotionMath.h"
#include "domain/SpeedCalculator.h"
#include "domain/TripCounter.h"
#include "input/ButtonInterpreter.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr uint32_t ZERO_SPEED_TIMEOUT_US = 1000000;

class FakeTimeSource : public core::TimeSource {
 public:
  uint32_t monotonicMilliseconds() const override { return nowMs; }
  void advance(uint32_t milliseconds) { nowMs += milliseconds; }
  uint32_t nowMs = 0;
};

struct Fixture {
  Fixture(uint32_t calibration = 1000)
      : clock(timeSource), application(clock, calibration,
                                       ZERO_SPEED_TIMEOUT_US) {}
  FakeTimeSource timeSource;
  core::SoftwareClock clock;
  core::ApplicationCore application;
};

core::ButtonEvent button(core::ButtonId id, core::ButtonEventType type,
                         uint32_t nowMs = 0) {
  return {id, type, nowMs};
}

void press(core::ApplicationCore& application, core::ButtonId id) {
  application.handleButton(button(id, core::ButtonEventType::Press));
}

void pulse(core::ApplicationCore& application, uint32_t count,
           uint32_t previousAtUs, uint32_t lastAtUs,
           uint32_t observedAtUs) {
  application.handleDistancePulses(
      {count, previousAtUs, lastAtUs, observedAtUs});
}

void acceptStartupTime(Fixture& fixture, uint8_t hour, uint8_t minute) {
  for (uint8_t index = 0; index < hour; ++index) {
    press(fixture.application, core::ButtonId::Up);
  }
  press(fixture.application, core::ButtonId::Right);
  for (uint8_t index = 0; index < minute; ++index) {
    press(fixture.application, core::ButtonId::Up);
  }
  press(fixture.application, core::ButtonId::Right);
}

void openMainAt(Fixture& fixture, uint8_t index) {
  press(fixture.application, core::ButtonId::Down);
  for (uint8_t position = 0; position < index; ++position) {
    press(fixture.application, core::ButtonId::Down);
  }
}

void assertClock(const core::ClockTime& time, uint8_t hour, uint8_t minute,
                 uint8_t second) {
  TEST_ASSERT_EQUAL_UINT8(hour, time.hour);
  TEST_ASSERT_EQUAL_UINT8(minute, time.minute);
  TEST_ASSERT_EQUAL_UINT8(second, time.second);
}

void assertSpeed(uint32_t millimetersPerPulse, uint32_t intervalUs,
                 float expectedKmh) {
  domain::SpeedCalculator calculator(millimetersPerPulse,
                                     ZERO_SPEED_TIMEOUT_US);
  calculator.update(intervalUs * 2, 2, intervalUs, intervalUs * 2);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, expectedKmh, calculator.speedKmh());
}

void testClockValidationBoundaries() {
  TEST_ASSERT_TRUE(core::isValidClockTime(0, 0));
  TEST_ASSERT_TRUE(core::isValidClockTime(7, 5));
  TEST_ASSERT_TRUE(core::isValidClockTime(12, 30));
  TEST_ASSERT_TRUE(core::isValidClockTime(23, 59));
  TEST_ASSERT_FALSE(core::isValidClockTime(24, 0));
  TEST_ASSERT_FALSE(core::isValidClockTime(0, 60));
}

void testClockDoesNotAdvanceBeforeAcceptance() {
  Fixture fixture;
  fixture.timeSource.advance(60000);
  TEST_ASSERT_FALSE(fixture.clock.isSet());
  assertClock(fixture.clock.now(), 0, 0, 0);
}

void testStartupAlwaysRequiresTimeAndCannotBeBypassed() {
  Fixture first;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartupTimeEntry),
                          static_cast<uint8_t>(first.application.screen()));
  press(first.application, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartupTimeEntry),
                          static_cast<uint8_t>(first.application.screen()));
  acceptStartupTime(first, 12, 34);
  Fixture restarted;
  TEST_ASSERT_FALSE(restarted.clock.isSet());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartupTimeEntry),
                          static_cast<uint8_t>(restarted.application.screen()));
}

void testTimeEntryNavigationAndBounds() {
  Fixture fixture;
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_UINT8(23, fixture.application.displayModel().timeEntry.hour);
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.application.displayModel().timeEntry.hour);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::TimeField::Minute),
                          static_cast<uint8_t>(fixture.application
                                                   .displayModel()
                                                   .timeEntry.activeField));
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_UINT8(59,
                          fixture.application.displayModel().timeEntry.minute);
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(0,
                          fixture.application.displayModel().timeEntry.minute);
  press(fixture.application, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::TimeField::Hour),
                          static_cast<uint8_t>(fixture.application
                                                   .displayModel()
                                                   .timeEntry.activeField));
}

void testAcceptedTimeStartsAtAcceptanceWithZeroSeconds() {
  Fixture fixture;
  fixture.timeSource.advance(12345);
  acceptStartupTime(fixture, 12, 34);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::BasicView),
                          static_cast<uint8_t>(fixture.application.screen()));
  assertClock(fixture.clock.now(), 12, 34, 0);
  fixture.timeSource.advance(1000);
  assertClock(fixture.clock.now(), 12, 34, 1);
  fixture.timeSource.advance(59000);
  assertClock(fixture.clock.now(), 12, 35, 0);
}

void testStartupAcceptsSpecifiedBoundaryExamples() {
  Fixture midnight;
  acceptStartupTime(midnight, 0, 0);
  assertClock(midnight.clock.now(), 0, 0, 0);
  Fixture morning;
  acceptStartupTime(morning, 7, 5);
  assertClock(morning.clock.now(), 7, 5, 0);
  Fixture midday;
  acceptStartupTime(midday, 12, 30);
  assertClock(midday.clock.now(), 12, 30, 0);
  Fixture endOfDay;
  acceptStartupTime(endOfDay, 23, 59);
  assertClock(endOfDay.clock.now(), 23, 59, 0);
}

void testClockHourDayAndMultiDayTransitions() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  clock.set(12, 59, 59);
  source.advance(1000);
  assertClock(clock.now(), 13, 0, 0);
  clock.set(23, 59, 59);
  source.advance(1000);
  assertClock(clock.now(), 0, 0, 0);
  clock.set(7, 5, 0);
  source.advance(3UL * 24UL * 60UL * 60UL * 1000UL + 2000UL);
  assertClock(clock.now(), 7, 5, 2);
}

void testClockMonotonicTimestampRollover() {
  FakeTimeSource source;
  source.nowMs = UINT32_MAX - 499U;
  core::SoftwareClock clock(source);
  clock.set(23, 59, 59);
  source.advance(1000);
  assertClock(clock.now(), 0, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(1000, clock.elapsedSinceSetMilliseconds());
}

void testBasicDisplayModelContainsOnlyBasicValues() {
  Fixture fixture;
  pulse(fixture.application, 2, 100000, 200000, 200000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartupTimeEntry),
                          static_cast<uint8_t>(
                              fixture.application.displayModel().screen));
  acceptStartupTime(fixture, 7, 5);
  const core::DisplayModel model = fixture.application.displayModel();
  assertClock(model.clock, 7, 5, 0);
  TEST_ASSERT_EQUAL_UINT64(2000, model.trip1.distanceMillimeters);
  TEST_ASSERT_EQUAL_UINT64(2000, model.trip2.distanceMillimeters);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, model.speedKmh);
  TEST_ASSERT_NULL(model.menu.title);
  TEST_ASSERT_EQUAL_UINT8(0, model.menu.visibleRowCount);
  TEST_ASSERT_EQUAL_UINT64(0, model.diagnostics.totalPulseCount);
}

void testWikiMenuOpeningSemanticsAndReturn() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::BasicView),
                          static_cast<uint8_t>(fixture.application.screen()));
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Menu),
                          static_cast<uint8_t>(fixture.application.screen()));
  TEST_ASSERT_EQUAL_UINT8(0, fixture.application.menuSelectedIndex());
  press(fixture.application, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::BasicView),
                          static_cast<uint8_t>(fixture.application.screen()));
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(7, fixture.application.menuSelectedIndex());
}

void testMainMenuWrapsAndSubmenuClamps() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(7, fixture.application.menuSelectedIndex());
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.application.menuSelectedIndex());
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::MenuPage::Order),
                          static_cast<uint8_t>(fixture.application.menuPage()));
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.application.menuSelectedIndex());
}

void testLongMainMenuScrollKeepsSelectionVisible() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  press(fixture.application, core::ButtonId::Up);
  const core::MenuDisplayModel menu = fixture.application.displayModel().menu;
  TEST_ASSERT_EQUAL_UINT8(7, menu.selectedIndex);
  TEST_ASSERT_EQUAL_UINT8(3, menu.scrollOffset);
  TEST_ASSERT_TRUE(menu.selectedVisibleRow < menu.visibleRowCount);
}

void testDisabledMenuItemCannotExecute() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Menu),
                          static_cast<uint8_t>(fixture.application.screen()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::MenuPage::Order),
                          static_cast<uint8_t>(fixture.application.menuPage()));
}

void testMenuTimeEditCancelAndAccept() {
  Fixture fixture;
  acceptStartupTime(fixture, 12, 34);
  openMainAt(fixture, 2);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::TimeEdit),
                          static_cast<uint8_t>(fixture.application.screen()));
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Left);
  press(fixture.application, core::ButtonId::Left);
  assertClock(fixture.clock.now(), 12, 34, 0);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  fixture.timeSource.advance(2500);
  press(fixture.application, core::ButtonId::Right);
  assertClock(fixture.clock.now(), 12, 35, 0);
  uint32_t ignored = 0;
  TEST_ASSERT_FALSE(fixture.application.takeCalibrationSaveRequest(ignored));
}

void testClockAndPulsesContinueInMenuAndDiagnostics() {
  Fixture fixture;
  acceptStartupTime(fixture, 1, 0);
  openMainAt(fixture, 7);
  fixture.timeSource.advance(1000);
  pulse(fixture.application, 1, 0, 100000, 100000);
  assertClock(fixture.application.displayModel().clock, 1, 0, 1);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  fixture.timeSource.advance(1000);
  pulse(fixture.application, 1, 100000, 200000, 200000);
  const core::DisplayModel diagnostics = fixture.application.displayModel();
  assertClock(diagnostics.clock, 1, 0, 2);
  TEST_ASSERT_EQUAL_UINT64(2, diagnostics.diagnostics.totalPulseCount);
  TEST_ASSERT_EQUAL_UINT64(2000, diagnostics.trip1.distanceMillimeters);
}

void testDiagnosticsContainsStatesAndDoesNotMutateDomain() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  pulse(fixture.application, 10, 900000, 1000000, 1000000);
  const uint64_t trip1 = fixture.application.trip1DistanceMillimeters();
  const uint32_t calibration = fixture.application.millimetersPerPulse();
  openMainAt(fixture, 7);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  fixture.application.handleButton(
      button(core::ButtonId::Up, core::ButtonEventType::Press));
  const core::DiagnosticsDisplayModel model =
      fixture.application.displayModel().diagnostics;
  TEST_ASSERT_TRUE(model.buttonPressed[1]);
  TEST_ASSERT_EQUAL_UINT64(10, model.totalPulseCount);
  TEST_ASSERT_EQUAL_UINT64(trip1, model.trip1DistanceMillimeters);
  TEST_ASSERT_EQUAL_UINT32(calibration, model.millimetersPerPulse);
  TEST_ASSERT_TRUE(model.clockSet);
  press(fixture.application, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::MenuPage::System),
                          static_cast<uint8_t>(fixture.application.menuPage()));
}

void testCalibrationValidationStepAndFallback() {
  TEST_ASSERT_FALSE(domain::calibration::isValid(0));
  TEST_ASSERT_TRUE(domain::calibration::isValid(
      CalibrationConfig::MIN_MILLIMETERS_PER_PULSE));
  TEST_ASSERT_TRUE(domain::calibration::isValid(
      CalibrationConfig::MAX_MILLIMETERS_PER_PULSE));
  TEST_ASSERT_FALSE(domain::calibration::isValid(UINT32_MAX));
  TEST_ASSERT_EQUAL_UINT32(1001, domain::calibration::increment(1000));
  TEST_ASSERT_EQUAL_UINT32(999, domain::calibration::decrement(1000));
}

void testCalibrationMenuCancelUnchangedAndChangedSave() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 3);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Left);
  uint32_t value = 0;
  TEST_ASSERT_FALSE(fixture.application.takeCalibrationSaveRequest(value));
  TEST_ASSERT_EQUAL_UINT32(1000, fixture.application.millimetersPerPulse());
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_FALSE(fixture.application.takeCalibrationSaveRequest(value));
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_TRUE(fixture.application.takeCalibrationSaveRequest(value));
  TEST_ASSERT_EQUAL_UINT32(1001, value);
  fixture.application.completeCalibrationSave(true);
  TEST_ASSERT_EQUAL_UINT32(1001, fixture.application.millimetersPerPulse());
}

void testCalibrationSaveFailureStaysInEditor() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 3);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Right);
  uint32_t value = 0;
  TEST_ASSERT_TRUE(fixture.application.takeCalibrationSaveRequest(value));
  fixture.application.completeCalibrationSave(false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::CalibrationEdit),
                          static_cast<uint8_t>(fixture.application.screen()));
  TEST_ASSERT_TRUE(
      fixture.application.displayModel().calibration.saveFailed);
}

void testTripResetsAreIndependentAndKeepTotal() {
  Fixture fixture;
  pulse(fixture.application, 10, 900000, 1000000, 1000000);
  fixture.application.handleButton(
      button(core::ButtonId::Trip1Reset, core::ButtonEventType::Press));
  TEST_ASSERT_EQUAL_UINT64(0, fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(10000,
                           fixture.application.trip2DistanceMillimeters());
  fixture.application.handleButton(
      button(core::ButtonId::Trip1Reset, core::ButtonEventType::Release));
  pulse(fixture.application, 1, 1000000, 1100000, 1100000);
  fixture.application.handleButton(
      button(core::ButtonId::Trip2Reset, core::ButtonEventType::Press));
  TEST_ASSERT_EQUAL_UINT64(1000,
                           fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(0, fixture.application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(11, fixture.application.totalPulseCount());
}

void testTripMenuActionsResetNamedTripOnly() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  pulse(fixture.application, 5, 400000, 500000, 500000);
  openMainAt(fixture, 6);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT64(0, fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(5000,
                           fixture.application.trip2DistanceMillimeters());
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT64(0, fixture.application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(5, fixture.application.totalPulseCount());
}

void testPulsesAccumulateDuringStartupTimeEntry() {
  Fixture fixture;
  pulse(fixture.application, 3, 200000, 300000, 300000);
  TEST_ASSERT_EQUAL_UINT64(3000,
                           fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(3000,
                           fixture.application.trip2DistanceMillimeters());
}

void testSparseAndDensePulseSpeeds() {
  assertSpeed(1000, 100000, 36.0F);
  assertSpeed(1000, 60000, 60.0F);
  assertSpeed(1000, 30000, 120.0F);
  assertSpeed(100, 10000, 36.0F);
  assertSpeed(100, 6000, 60.0F);
  assertSpeed(100, 3000, 120.0F);
}

void testSpeedReturnsToZeroAndTimestampRollover() {
  domain::SpeedCalculator calculator(1000, ZERO_SPEED_TIMEOUT_US);
  constexpr uint32_t previous = UINT32_MAX - 49999U;
  constexpr uint32_t last = 50000U;
  calculator.update(last, 2, previous, last);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 36.0F, calculator.speedKmh());
  calculator.update(last + ZERO_SPEED_TIMEOUT_US, 2, previous, last);
  TEST_ASSERT_EQUAL_FLOAT(0.0F, calculator.speedKmh());
}

void testGeneratorCycleDistanceSpeedAndPauses() {
  Fixture fixture;
  pulse(fixture.application, 300, 29900000, 30000000, 30000000);
  TEST_ASSERT_FLOAT_WITHIN(
      0.01F, 36.0F, fixture.application.displayModel().speedKmh);
  fixture.application.tick(40000000);
  TEST_ASSERT_EQUAL_FLOAT(0.0F,
                          fixture.application.displayModel().speedKmh);
  pulse(fixture.application, 500, 69940000, 70000000, 70000000);
  TEST_ASSERT_FLOAT_WITHIN(
      0.01F, 60.0F, fixture.application.displayModel().speedKmh);
  fixture.application.tick(80000000);
  pulse(fixture.application, 1000, 109970000, 110000000, 110000000);
  TEST_ASSERT_FLOAT_WITHIN(
      0.01F, 120.0F, fixture.application.displayModel().speedKmh);
  fixture.application.tick(120000000);
  TEST_ASSERT_EQUAL_UINT64(1800000,
                           fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(1800000,
                           fixture.application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(1800, fixture.application.totalPulseCount());
  TEST_ASSERT_EQUAL_FLOAT(0.0F,
                          fixture.application.displayModel().speedKmh);
}

void testCalibrationChangeAffectsOnlyFuturePulses() {
  domain::TripCounter trip(1000);
  trip.addPulses(1);
  trip.setMillimetersPerPulse(100);
  trip.addPulses(1);
  TEST_ASSERT_EQUAL_UINT64(1100, trip.distanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(2, trip.pulseCount());
}

void testDistanceMathAndLongRunSaturate() {
  TEST_ASSERT_EQUAL_UINT64(
      static_cast<uint64_t>(UINT32_MAX) * UINT32_MAX,
      domain::motion::distanceMillimetersForPulses(UINT32_MAX, UINT32_MAX));
  TEST_ASSERT_EQUAL_UINT64(
      std::numeric_limits<uint64_t>::max(),
      domain::motion::saturatingAdd(
          std::numeric_limits<uint64_t>::max() - 5, 6));
  Fixture fixture(CalibrationConfig::MAX_MILLIMETERS_PER_PULSE);
  for (uint32_t index = 0; index < 43000; ++index) {
    pulse(fixture.application, UINT32_MAX, 1, 2, 2);
  }
  TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                           fixture.application.trip1DistanceMillimeters());
}

void testButtonDebounceAndLongRepeatSemantics() {
  input::ButtonInterpreter interpreter(35, 500, 100);
  interpreter.reset(false, 0);
  interpreter.update(true, 10);
  TEST_ASSERT_TRUE(interpreter.update(true, 45).pressed);
  TEST_ASSERT_TRUE(interpreter.update(true, 545).longStart);
  TEST_ASSERT_TRUE(interpreter.update(true, 645).longRepeat);
  interpreter.update(false, 700);
  const input::ButtonTransitions released = interpreter.update(false, 735);
  TEST_ASSERT_TRUE(released.released);
  TEST_ASSERT_FALSE(released.shortPress);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testClockValidationBoundaries);
  RUN_TEST(testClockDoesNotAdvanceBeforeAcceptance);
  RUN_TEST(testStartupAlwaysRequiresTimeAndCannotBeBypassed);
  RUN_TEST(testTimeEntryNavigationAndBounds);
  RUN_TEST(testAcceptedTimeStartsAtAcceptanceWithZeroSeconds);
  RUN_TEST(testStartupAcceptsSpecifiedBoundaryExamples);
  RUN_TEST(testClockHourDayAndMultiDayTransitions);
  RUN_TEST(testClockMonotonicTimestampRollover);
  RUN_TEST(testBasicDisplayModelContainsOnlyBasicValues);
  RUN_TEST(testWikiMenuOpeningSemanticsAndReturn);
  RUN_TEST(testMainMenuWrapsAndSubmenuClamps);
  RUN_TEST(testLongMainMenuScrollKeepsSelectionVisible);
  RUN_TEST(testDisabledMenuItemCannotExecute);
  RUN_TEST(testMenuTimeEditCancelAndAccept);
  RUN_TEST(testClockAndPulsesContinueInMenuAndDiagnostics);
  RUN_TEST(testDiagnosticsContainsStatesAndDoesNotMutateDomain);
  RUN_TEST(testCalibrationValidationStepAndFallback);
  RUN_TEST(testCalibrationMenuCancelUnchangedAndChangedSave);
  RUN_TEST(testCalibrationSaveFailureStaysInEditor);
  RUN_TEST(testTripResetsAreIndependentAndKeepTotal);
  RUN_TEST(testTripMenuActionsResetNamedTripOnly);
  RUN_TEST(testPulsesAccumulateDuringStartupTimeEntry);
  RUN_TEST(testSparseAndDensePulseSpeeds);
  RUN_TEST(testSpeedReturnsToZeroAndTimestampRollover);
  RUN_TEST(testGeneratorCycleDistanceSpeedAndPauses);
  RUN_TEST(testCalibrationChangeAffectsOnlyFuturePulses);
  RUN_TEST(testDistanceMathAndLongRunSaturate);
  RUN_TEST(testButtonDebounceAndLongRepeatSemantics);
  return UNITY_END();
}
