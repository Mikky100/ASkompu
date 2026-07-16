#include <unity.h>

#include <cstdint>
#include <limits>

#include "BoardConfig.h"
#include "core/ApplicationCore.h"
#include "core/Clock.h"
#include "domain/CompetitionEngine.h"
#include "domain/MotionMath.h"
#include "domain/TripCounter.h"
#include "input/ButtonInterpreter.h"
#include "input/StableSignalFilter.h"

void setUp() {}
void tearDown() {}

namespace {

domain::SegmentDefinition segment(uint16_t index, domain::SegmentType type,
                                  uint32_t value,
                                  domain::PointType end = domain::PointType::NORMAL) {
  domain::SegmentDefinition result;
  result.segmentIndex = index;
  result.startPointIndex = index;
  result.endPointIndex = index + 1;
  result.segmentType = type;
  result.value = value;
  result.pointTypeAtEnd = end;
  return result;
}

domain::RouteOrder orderWith(const domain::SegmentDefinition& first) {
  domain::RouteOrder order;
  order.startHour = 12;
  order.startMinute = 0;
  order.segments.push_back(first);
  order.segments.back().pointTypeAtEnd = domain::PointType::FINISH_M;
  return order;
}

domain::RouteOrder twoSegmentOrder(domain::SegmentType firstType,
                                   uint32_t firstValue,
                                   domain::SegmentType secondType,
                                   uint32_t secondValue) {
  domain::RouteOrder order;
  order.startHour = 12;
  order.startMinute = 0;
  order.segments.push_back(segment(0, firstType, firstValue));
  order.segments.push_back(
      segment(1, secondType, secondValue, domain::PointType::FINISH_M));
  return order;
}

void activateAtNoon(domain::CompetitionEngine& engine,
                    const domain::RouteOrder& order) {
  TEST_ASSERT_TRUE(engine.activate(order, 12UL * 3600UL * 1000UL, 0));
}

void testWaitStartPositiveAndAutomaticStart() {
  domain::CompetitionEngine engine;
  domain::RouteOrder order = orderWith(segment(0, domain::SegmentType::TIME, 60));
  order.startMinute = 1;
  TEST_ASSERT_TRUE(engine.activate(order, 12UL * 3600UL * 1000UL, 1000));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::WAIT_START),
                          static_cast<uint8_t>(engine.state()));
  TEST_ASSERT_EQUAL_INT64(120000, engine.deltaMs());
  engine.tick(61000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::RUNNING),
                          static_cast<uint8_t>(engine.state()));
  TEST_ASSERT_EQUAL_INT64(60000, engine.deltaMs());
}

void testPastStartAndMidnightTimeline() {
  domain::CompetitionEngine past;
  domain::RouteOrder order = orderWith(segment(0, domain::SegmentType::TIME, 30));
  order.startMinute = 59;
  TEST_ASSERT_TRUE(past.activate(order, (13UL * 3600UL) * 1000UL, 0));
  TEST_ASSERT_EQUAL_INT64(-30000, past.deltaMs());

  domain::CompetitionEngine midnight;
  order.startHour = 0;
  order.startMinute = 1;
  TEST_ASSERT_TRUE(midnight.activate(order,
                                     (23UL * 3600UL + 59UL * 60UL) * 1000UL,
                                     0));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::WAIT_START),
                          static_cast<uint8_t>(midnight.state()));
  midnight.tick(120000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::RUNNING),
                          static_cast<uint8_t>(midnight.state()));
}

void testTimeSegmentsRemainCumulative() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine, twoSegmentOrder(domain::SegmentType::TIME, 10,
                                         domain::SegmentType::TIME, 20));
  engine.tick(12000);
  TEST_ASSERT_EQUAL_INT64(-2000, engine.deltaMs());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::ADVANCED),
                          static_cast<uint8_t>(engine.pointReleased(12000)));
  TEST_ASSERT_EQUAL_INT64(18000, engine.deltaMs());
  engine.tick(35000);
  TEST_ASSERT_EQUAL_INT64(-5000, engine.deltaMs());
  TEST_ASSERT_EQUAL_INT64(-5, engine.deltaSeconds());
}

void testSpeedFixedPointAtGeneratorSpeeds() {
  const uint64_t distancesMm[] = {300000, 500000, 1000000};
  const int64_t expectedDeltaMs[] = {-12000, 0, 30000};
  for (uint8_t i = 0; i < 3; ++i) {
    domain::CompetitionEngine engine;
    activateAtNoon(engine,
                   orderWith(segment(0, domain::SegmentType::SPEED, 60)));
    engine.addDistanceMillimeters(distancesMm[i]);
    engine.tick(30000);
    TEST_ASSERT_EQUAL_INT64(expectedDeltaMs[i], engine.deltaMs());
  }
}

void testCalibrationExamplesAndTripIndependence() {
  domain::CompetitionEngine oneMeter;
  activateAtNoon(oneMeter,
                 orderWith(segment(0, domain::SegmentType::SPEED, 36)));
  for (uint8_t i = 0; i < 10; ++i) oneMeter.addDistanceMillimeters(1000);
  TEST_ASSERT_EQUAL_UINT64(10000, oneMeter.segmentDistanceMillimeters());

  domain::CompetitionEngine tenthMeter;
  activateAtNoon(tenthMeter,
                 orderWith(segment(0, domain::SegmentType::SPEED, 36)));
  for (uint8_t i = 0; i < 10; ++i) tenthMeter.addDistanceMillimeters(100);
  TEST_ASSERT_EQUAL_UINT64(1000, tenthMeter.segmentDistanceMillimeters());
}

void testPointResetsOnlySegmentAndCanUndo() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine, twoSegmentOrder(domain::SegmentType::SPEED, 36,
                                         domain::SegmentType::TIME, 20));
  engine.addDistanceMillimeters(10000);
  engine.tick(1000);
  const int64_t before = engine.deltaMs();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::ADVANCED),
                          static_cast<uint8_t>(engine.pointReleased(1000)));
  TEST_ASSERT_EQUAL_UINT16(1, engine.currentSegmentIndex());
  TEST_ASSERT_EQUAL_UINT64(0, engine.segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(10000, engine.stageDistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(10000, engine.physicalDistanceMillimeters());
  TEST_ASSERT_TRUE(engine.hasUndoPrompt(3999));
  TEST_ASSERT_TRUE(engine.undoPoint(3999));
  TEST_ASSERT_EQUAL_UINT16(0, engine.currentSegmentIndex());
  TEST_ASSERT_EQUAL_UINT64(10000, engine.segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(before - 2999, engine.deltaMs());
}

void testPointUndoExpires() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine, twoSegmentOrder(domain::SegmentType::TIME, 10,
                                         domain::SegmentType::TIME, 20));
  engine.pointReleased(0);
  TEST_ASSERT_FALSE(engine.undoPoint(3001));
  TEST_ASSERT_EQUAL_UINT16(1, engine.currentSegmentIndex());
}

void testJatDoesNotAdvanceOrMutateCalculation() {
  domain::RouteOrder order = twoSegmentOrder(domain::SegmentType::TIME, 10,
                                             domain::SegmentType::TIME, 20);
  order.segments[0].pointTypeAtEnd = domain::PointType::JAT;
  order.segments[0].hasJatType = true;
  order.segments[0].jatType = domain::JatType::EMIT_MLA;
  domain::CompetitionEngine engine;
  activateAtNoon(engine, order);
  engine.tick(5000);
  const int64_t delta = engine.deltaMs();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::PointResult::JAT_NOT_IMPLEMENTED),
      static_cast<uint8_t>(engine.pointReleased(5000)));
  TEST_ASSERT_EQUAL_UINT16(0, engine.currentSegmentIndex());
  TEST_ASSERT_EQUAL_INT64(delta, engine.deltaMs());
  TEST_ASSERT_TRUE(engine.jatNotImplemented());
}

void testFinishFreezesAgainstTimeAndPulses() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine,
                 orderWith(segment(0, domain::SegmentType::SPEED, 36)));
  engine.addDistanceMillimeters(10000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::FINISHED),
                          static_cast<uint8_t>(engine.pointReleased(1000)));
  const int64_t delta = engine.deltaMs();
  engine.tick(999999);
  engine.addDistanceMillimeters(999999);
  TEST_ASSERT_EQUAL_INT64(delta, engine.deltaMs());
  TEST_ASSERT_EQUAL_UINT64(10000, engine.segmentDistanceMillimeters());
  TEST_ASSERT_TRUE(engine.deltaFrozen());
}

void testLargeDistanceSaturatesWithoutOverflow() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine,
                 orderWith(segment(0, domain::SegmentType::SPEED, 1)));
  engine.addDistanceMillimeters(std::numeric_limits<int64_t>::max() - 1);
  engine.addDistanceMillimeters(10);
  TEST_ASSERT_EQUAL_INT64(std::numeric_limits<int64_t>::max(),
                          engine.segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(std::numeric_limits<int64_t>::max(),
                          engine.idealTimeMs());
}

class FakeTimeSource : public core::TimeSource {
 public:
  uint32_t monotonicMilliseconds() const override { return nowMs; }
  uint32_t nowMs = 0;
};

void press(core::ApplicationCore& app, core::ButtonId id,
           core::ButtonEventType type = core::ButtonEventType::Press,
           uint32_t ms = 0) {
  app.handleButton({id, type, ms});
}

void acceptNoon(core::ApplicationCore& app) {
  for (uint8_t i = 0; i < 12; ++i) press(app, core::ButtonId::Up);
  press(app, core::ButtonId::Right);
  press(app, core::ButtonId::Right);
}

const domain::RouteOrder* createFutureOrderSaveRequest(
    core::ApplicationCore& app) {
  press(app, core::ButtonId::Down);   // main menu, first item
  press(app, core::ButtonId::Right);  // create order
  press(app, core::ButtonId::Right);  // competition type
  for (uint8_t i = 0; i < 12; ++i) press(app, core::ButtonId::Up);
  press(app, core::ButtonId::Right);  // minutes
  press(app, core::ButtonId::Up);     // 12:01
  press(app, core::ButtonId::Right);  // first segment
  press(app, core::ButtonId::Right);  // TIME value
  press(app, core::ButtonId::Right);
  press(app, core::ButtonId::Right);
  press(app, core::ButtonId::Right);
  press(app, core::ButtonId::Up);     // 00:01
  press(app, core::ButtonId::Right);  // continuation
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Down);   // finish
  press(app, core::ButtonId::Right);
  const domain::RouteOrder* request = nullptr;
  TEST_ASSERT_TRUE(app.takeRouteOrderSaveRequest(request));
  return request;
}

void testSuccessfulSaveActivatesWaitStartAndFailureDoesNot() {
  FakeTimeSource successSource;
  core::SoftwareClock successClock(successSource);
  core::ApplicationCore success(successClock, 1000, 1000000);
  acceptNoon(success);
  TEST_ASSERT_NOT_NULL(createFutureOrderSaveRequest(success));
  success.completeRouteOrderSave(true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::WAIT_START),
                          static_cast<uint8_t>(success.competition().state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::BasicView),
                          static_cast<uint8_t>(success.screen()));

  FakeTimeSource failureSource;
  core::SoftwareClock failureClock(failureSource);
  core::ApplicationCore failure(failureClock, 1000, 1000000);
  acceptNoon(failure);
  TEST_ASSERT_NOT_NULL(createFutureOrderSaveRequest(failure));
  failure.completeRouteOrderSave(false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::IDLE),
                          static_cast<uint8_t>(failure.competition().state()));
}

void testActiveStoredOrderStartsAfterClockAcceptance() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::TIME, 60)),
                           true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::IDLE),
                          static_cast<uint8_t>(app.competition().state()));
  acceptNoon(app);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::RUNNING),
                          static_cast<uint8_t>(app.competition().state()));
}

void testInactiveStoredOrderDoesNotRestart() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::TIME, 60)),
                           false);
  acceptNoon(app);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::IDLE),
                          static_cast<uint8_t>(app.competition().state()));
}

void testApplicationPointAndTripResetIntegration() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(twoSegmentOrder(domain::SegmentType::SPEED, 36,
                                           domain::SegmentType::TIME, 20), true);
  acceptNoon(app);
  app.handleDistancePulses({10, 0, 1000, 1000});
  press(app, core::ButtonId::Trip1Reset);
  TEST_ASSERT_EQUAL_UINT64(0, app.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(10000,
                           app.competition().segmentDistanceMillimeters());
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT16(1, app.competition().currentSegmentIndex());
}

void testFinishRequestsPersistentCompletion() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::TIME, 1)),
                           true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_TRUE(app.takeRouteOrderCompletionRequest());
  TEST_ASSERT_FALSE(app.takeRouteOrderCompletionRequest());
  app.completeRouteOrderCompletion(true);
  TEST_ASSERT_FALSE(app.takeRouteOrderCompletionRequest());
}

void testPulseGeneratorCycleIntegration() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(
      orderWith(segment(0, domain::SegmentType::SPEED, 60)), true);
  acceptNoon(app);
  app.handleDistancePulses({300, 29900000, 30000000, 30000000});
  source.nowMs = 30000;
  app.tick(30000000);
  source.nowMs = 40000;
  app.tick(40000000);
  app.handleDistancePulses({500, 69940000, 70000000, 70000000});
  source.nowMs = 70000;
  app.tick(70000000);
  source.nowMs = 80000;
  app.tick(80000000);
  app.handleDistancePulses({1000, 109970000, 110000000, 110000000});
  source.nowMs = 110000;
  app.tick(110000000);
  source.nowMs = 120000;
  app.tick(120000000);
  TEST_ASSERT_EQUAL_UINT64(1800000, app.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(1800000,
                           app.competition().segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(-12000, app.competition().deltaMs());
}

void testLilyGoCompetitionGPIOContractAndCoreEvents() {
  TEST_ASSERT_EQUAL_UINT8(1, BoardConfig::PIN_BUTTON_LEFT);
  TEST_ASSERT_EQUAL_UINT8(2, BoardConfig::PIN_BUTTON_UP);
  TEST_ASSERT_EQUAL_UINT8(3, BoardConfig::PIN_BUTTON_DOWN);
  TEST_ASSERT_EQUAL_UINT8(10, BoardConfig::PIN_BUTTON_RIGHT);
  TEST_ASSERT_EQUAL_UINT8(11, BoardConfig::PIN_BUTTON_TRIP2_RESET);
  TEST_ASSERT_EQUAL_UINT8(12, BoardConfig::PIN_BUTTON_AT);
  TEST_ASSERT_EQUAL_UINT8(13, BoardConfig::PIN_REVERSE_INPUT);
  TEST_ASSERT_EQUAL_UINT8(14, BoardConfig::PIN_BUTTON_POINT);
  TEST_ASSERT_EQUAL_UINT8(16, BoardConfig::PIN_PULSE_INPUT);
  TEST_ASSERT_TRUE(core::ButtonId::Point != core::ButtonId::Trip1Reset);
  TEST_ASSERT_TRUE(core::ButtonId::At != core::ButtonId::Point);
}

void testPointButtonTimingProducesShortOrLongExclusively() {
  input::ButtonInterpreter shortPress(35, 1200, 0);
  shortPress.reset(false, 0);
  TEST_ASSERT_FALSE(shortPress.update(true, 10).pressed);
  TEST_ASSERT_TRUE(shortPress.update(true, 45).pressed);
  shortPress.update(false, 100);
  const input::ButtonTransitions shortReleased = shortPress.update(false, 135);
  TEST_ASSERT_TRUE(shortReleased.released);
  TEST_ASSERT_TRUE(shortReleased.shortPress);

  input::ButtonInterpreter longPress(35, 1200, 0);
  longPress.reset(false, 0);
  longPress.update(true, 10);
  longPress.update(true, 45);
  TEST_ASSERT_TRUE(longPress.update(true, 1245).longStart);
  TEST_ASSERT_FALSE(longPress.update(true, 2000).longRepeat);
  longPress.update(false, 2100);
  const input::ButtonTransitions longReleased = longPress.update(false, 2135);
  TEST_ASSERT_TRUE(longReleased.released);
  TEST_ASSERT_FALSE(longReleased.shortPress);
}

void testPointPressLongAndAtDoNotMutateCompetition() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(twoSegmentOrder(domain::SegmentType::TIME, 10,
                                           domain::SegmentType::TIME, 20), true);
  acceptNoon(app);
  const int64_t delta = app.competition().deltaMs();
  press(app, core::ButtonId::Point, core::ButtonEventType::Press);
  TEST_ASSERT_EQUAL_UINT16(0, app.competition().currentSegmentIndex());
  press(app, core::ButtonId::Point, core::ButtonEventType::LongStart);
  TEST_ASSERT_EQUAL_UINT16(0, app.competition().currentSegmentIndex());
  TEST_ASSERT_EQUAL_INT64(delta, app.competition().deltaMs());
  TEST_ASSERT_TRUE(
      app.displayModel().competition.pointLongPressNotImplemented);
  press(app, core::ButtonId::At, core::ButtonEventType::Press);
  TEST_ASSERT_FALSE(app.displayModel().competition.atNotImplemented);
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  TEST_ASSERT_TRUE(app.displayModel().competition.atNotImplemented);
  TEST_ASSERT_EQUAL_UINT16(0, app.competition().currentSegmentIndex());
  TEST_ASSERT_EQUAL_INT64(delta, app.competition().deltaMs());
}

void testPointReleaseAdvancesExactlyOnceAndTrip1KeepsAccumulating() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(twoSegmentOrder(domain::SegmentType::TIME, 10,
                                           domain::SegmentType::TIME, 20), true);
  acceptNoon(app);
  app.handleDistancePulses({2, 0, 1000, 1000, false});
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT16(1, app.competition().currentSegmentIndex());
  TEST_ASSERT_EQUAL_INT64(2000, app.trip1DistanceMillimeters());
  app.handleDistancePulses({1, 1000, 2000, 2000, false});
  TEST_ASSERT_EQUAL_INT64(3000, app.trip1DistanceMillimeters());
}

void testTrip2ResetKeepsAllCompetitionAndTrip1Values() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::SPEED, 36)),
                           true);
  acceptNoon(app);
  app.handleDistancePulses({5, 0, 1000, 1000, false});
  const int64_t competitionDistance =
      app.competition().segmentDistanceMillimeters();
  const uint64_t pulses = app.totalPulseCount();
  press(app, core::ButtonId::Trip2Reset);
  TEST_ASSERT_EQUAL_INT64(5000, app.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(0, app.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(competitionDistance,
                          app.competition().segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(pulses, app.totalPulseCount());
}

void testReversePulsesAreSignedAcrossAllDistancesAndDirectionChanges() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::SPEED, 36)),
                           true);
  acceptNoon(app);
  app.handleDistancePulses({3, 0, 1000, 1000, false});
  app.handleDistancePulses({5, 1000, 2000, 2000, true});
  app.handleDistancePulses({1, 2000, 3000, 3000, false});
  TEST_ASSERT_EQUAL_INT64(-1000, app.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(-1000, app.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(-1000,
                          app.competition().physicalDistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(-1000,
                          app.competition().stageDistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(-1000,
                          app.competition().segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(9, app.totalPulseCount());
  TEST_ASSERT_FALSE(app.competition().reverseActive());
}

void testReverseNearZeroAndSignedSaturation() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine,
                 orderWith(segment(0, domain::SegmentType::SPEED, 36)));
  engine.addDistanceMillimeters(-1);
  TEST_ASSERT_EQUAL_INT64(-1, engine.segmentDistanceMillimeters());
  engine.addDistanceMillimeters(std::numeric_limits<int64_t>::min());
  TEST_ASSERT_EQUAL_INT64(std::numeric_limits<int64_t>::min(),
                          engine.segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(std::numeric_limits<int64_t>::min(),
                          domain::motion::saturatingAddSigned(
                              std::numeric_limits<int64_t>::min() + 1, -2));
}

void testReverseSignalFilterRejectsGlitchesAndReportsStableChanges() {
  input::StableSignalFilter filter(20);
  filter.reset(false, 0);
  TEST_ASSERT_FALSE(filter.update(true, 5));
  TEST_ASSERT_FALSE(filter.update(false, 15));
  TEST_ASSERT_FALSE(filter.active());
  TEST_ASSERT_FALSE(filter.update(true, 20));
  TEST_ASSERT_FALSE(filter.update(true, 39));
  TEST_ASSERT_TRUE(filter.update(true, 40));
  TEST_ASSERT_TRUE(filter.active());
  TEST_ASSERT_FALSE(filter.update(false, 45));
  TEST_ASSERT_TRUE(filter.update(false, 65));
  TEST_ASSERT_FALSE(filter.active());
}

void testShortButtonGlitchAndDiagnosticsSignals() {
  input::ButtonInterpreter glitch(35, 1200, 0);
  glitch.reset(false, 0);
  TEST_ASSERT_FALSE(glitch.update(true, 5).pressed);
  TEST_ASSERT_FALSE(glitch.update(false, 25).released);
  TEST_ASSERT_FALSE(glitch.update(false, 60).released);

  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  acceptNoon(app);
  press(app, core::ButtonId::Up);     // main menu at system
  press(app, core::ButtonId::Right);  // system submenu
  press(app, core::ButtonId::Right);  // diagnostics
  app.handleReverseSignal({true, 10});
  press(app, core::ButtonId::Point, core::ButtonEventType::LongStart);
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  const core::DisplayModel model = app.displayModel();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::ButtonEventType::LongStart),
      model.diagnostics.lastPointEventType);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::ButtonEventType::Release),
      model.diagnostics.lastAtEventType);
  TEST_ASSERT_TRUE(model.diagnostics.reverseActive);
  TEST_ASSERT_TRUE(app.competition().reverseActive());
}

void testTripCounterSupportsSignedReverseDistance() {
  domain::TripCounter trip(1000);
  trip.addPulses(2);
  trip.addPulses(3, true);
  TEST_ASSERT_EQUAL_INT64(-1000, trip.distanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(5, trip.pulseCount());
}

void testWaitStartOrderMenuPromptsEditOrReplace() {
  domain::RouteOrder future =
      orderWith(segment(0, domain::SegmentType::TIME, 60));
  future.startMinute = 1;

  FakeTimeSource editSource;
  core::SoftwareClock editClock(editSource);
  core::ApplicationCore editApp(editClock, 1000, 1000000);
  editApp.setInitialRouteOrder(future, true);
  acceptNoon(editApp);
  press(editApp, core::ButtonId::Down);
  press(editApp, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::Screen::OrderAccessPrompt),
      static_cast<uint8_t>(editApp.screen()));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::OrderAccessAction::Edit),
      static_cast<uint8_t>(
          editApp.displayModel().orderAccess.selectedAction));
  press(editApp, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::OrderEdit),
                          static_cast<uint8_t>(editApp.screen()));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::BROWSE),
      static_cast<uint8_t>(editApp.displayModel().order.editor.phase));

  FakeTimeSource replaceSource;
  core::SoftwareClock replaceClock(replaceSource);
  core::ApplicationCore replaceApp(replaceClock, 1000, 1000000);
  replaceApp.setInitialRouteOrder(future, true);
  acceptNoon(replaceApp);
  press(replaceApp, core::ButtonId::Down);
  press(replaceApp, core::ButtonId::Right);
  press(replaceApp, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::OrderAccessAction::Replace),
      static_cast<uint8_t>(
          replaceApp.displayModel().orderAccess.selectedAction));
  press(replaceApp, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::COMPETITION_TYPE),
      static_cast<uint8_t>(replaceApp.displayModel().order.editor.phase));
}

void testRunningOrderMenuUsesSameAccessPrompt() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::TIME, 60)),
                           true);
  acceptNoon(app);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::RUNNING),
                          static_cast<uint8_t>(app.competition().state()));
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::Screen::OrderAccessPrompt),
      static_cast<uint8_t>(app.screen()));
  press(app, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Menu),
                          static_cast<uint8_t>(app.screen()));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testWaitStartPositiveAndAutomaticStart);
  RUN_TEST(testPastStartAndMidnightTimeline);
  RUN_TEST(testTimeSegmentsRemainCumulative);
  RUN_TEST(testSpeedFixedPointAtGeneratorSpeeds);
  RUN_TEST(testCalibrationExamplesAndTripIndependence);
  RUN_TEST(testPointResetsOnlySegmentAndCanUndo);
  RUN_TEST(testPointUndoExpires);
  RUN_TEST(testJatDoesNotAdvanceOrMutateCalculation);
  RUN_TEST(testFinishFreezesAgainstTimeAndPulses);
  RUN_TEST(testLargeDistanceSaturatesWithoutOverflow);
  RUN_TEST(testActiveStoredOrderStartsAfterClockAcceptance);
  RUN_TEST(testInactiveStoredOrderDoesNotRestart);
  RUN_TEST(testSuccessfulSaveActivatesWaitStartAndFailureDoesNot);
  RUN_TEST(testApplicationPointAndTripResetIntegration);
  RUN_TEST(testFinishRequestsPersistentCompletion);
  RUN_TEST(testPulseGeneratorCycleIntegration);
  RUN_TEST(testLilyGoCompetitionGPIOContractAndCoreEvents);
  RUN_TEST(testPointButtonTimingProducesShortOrLongExclusively);
  RUN_TEST(testPointPressLongAndAtDoNotMutateCompetition);
  RUN_TEST(testPointReleaseAdvancesExactlyOnceAndTrip1KeepsAccumulating);
  RUN_TEST(testTrip2ResetKeepsAllCompetitionAndTrip1Values);
  RUN_TEST(testReversePulsesAreSignedAcrossAllDistancesAndDirectionChanges);
  RUN_TEST(testReverseNearZeroAndSignedSaturation);
  RUN_TEST(testReverseSignalFilterRejectsGlitchesAndReportsStableChanges);
  RUN_TEST(testShortButtonGlitchAndDiagnosticsSignals);
  RUN_TEST(testTripCounterSupportsSignedReverseDistance);
  RUN_TEST(testWaitStartOrderMenuPromptsEditOrReplace);
  RUN_TEST(testRunningOrderMenuUsesSameAccessPrompt);
  return UNITY_END();
}
