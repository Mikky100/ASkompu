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

domain::RouteOrder mittisThenTimeOrder(uint32_t referenceMeters = 1000,
                                       uint16_t durationSeconds = 60) {
  domain::RouteOrder order =
      twoSegmentOrder(domain::SegmentType::MITTIS, referenceMeters,
                      domain::SegmentType::TIME, 60);
  order.segments[0].hasMittisDuration = true;
  order.segments[0].mittisDurationSeconds = durationSeconds;
  return order;
}

domain::RouteOrder jatThenFinishOrder(domain::JatType jatType,
                                      int16_t offsetMinutes = 0) {
  domain::RouteOrder order =
      twoSegmentOrder(domain::SegmentType::TIME, 10,
                      domain::SegmentType::TIME, 20);
  order.competitionType = jatType == domain::JatType::MANNED_JAT
                              ? domain::CompetitionType::NON_EMIT
                              : domain::CompetitionType::EMIT;
  domain::SegmentDefinition& first = order.segments[0];
  first.pointTypeAtEnd = domain::PointType::JAT;
  first.hasJatType = true;
  first.jatType = jatType;
  first.hasJatOffsetMinutes =
      jatType == domain::JatType::MANNED_JAT ||
      jatType == domain::JatType::EMIT_JAT_OFFSET;
  first.jatOffsetMinutes = offsetMinutes;
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

void testMittisUsesDurationAndFreezesMeasuredInterval() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine, mittisThenTimeOrder(1000, 75));
  engine.addDistanceMillimeters(500000);
  engine.tick(10000);
  TEST_ASSERT_EQUAL_INT64(65000, engine.deltaMs());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::PointResult::MITTIS_PROPOSAL),
      static_cast<uint8_t>(engine.pointReleased(10000, 1000)));
  TEST_ASSERT_TRUE(engine.mittisProposal().pending);
  TEST_ASSERT_TRUE(engine.mittisProposal().valid);
  TEST_ASSERT_EQUAL_UINT32(2000,
                           engine.mittisProposal().proposedMillimetersPerPulse);
  engine.addDistanceMillimeters(3000);
  engine.tick(15000);
  TEST_ASSERT_EQUAL_INT64(500000, engine.segmentDistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::ADVANCED),
                          static_cast<uint8_t>(engine.resolveMittisProposal()));
  TEST_ASSERT_EQUAL_INT64(3000, engine.segmentDistanceMillimeters());
}

void testMittisZeroDistanceIsRejectedSafely() {
  domain::CompetitionEngine engine;
  activateAtNoon(engine, mittisThenTimeOrder());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::PointResult::MITTIS_PROPOSAL),
      static_cast<uint8_t>(engine.pointReleased(1000, 1000)));
  TEST_ASSERT_FALSE(engine.mittisProposal().valid);
  TEST_ASSERT_EQUAL_UINT32(0,
                           engine.mittisProposal().proposedMillimetersPerPulse);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::ADVANCED),
                          static_cast<uint8_t>(engine.resolveMittisProposal()));
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

void testJatCompletesStageAtomically() {
  domain::RouteOrder order = twoSegmentOrder(domain::SegmentType::TIME, 10,
                                             domain::SegmentType::TIME, 20);
  order.segments[0].pointTypeAtEnd = domain::PointType::JAT;
  order.segments[0].hasJatType = true;
  order.segments[0].jatType = domain::JatType::EMIT_MLA;
  domain::CompetitionEngine engine;
  activateAtNoon(engine, order);
  engine.tick(5000);
  const int64_t delta = engine.deltaMs();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::JAT_COMPLETED),
                          static_cast<uint8_t>(engine.pointReleased(
                              5000, 1000, {12, 0, 5}, {})));
  TEST_ASSERT_EQUAL_UINT16(1, engine.currentSegmentIndex());
  TEST_ASSERT_EQUAL_INT64(delta, engine.deltaMs());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::JAT_RESULT),
                          static_cast<uint8_t>(engine.state()));
  TEST_ASSERT_TRUE(engine.deltaFrozen());
  TEST_ASSERT_EQUAL_size_t(1, engine.stageResults().size());
  TEST_ASSERT_EQUAL_INT64(delta / 1000,
                          engine.stageResults()[0].finalDeltaSeconds);
}

void testJatResultTimeoutZeroAndMinuteProposalBoundaries() {
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 5;
  domain::CompetitionEngine exactTwenty;
  activateAtNoon(exactTwenty,
                 jatThenFinishOrder(domain::JatType::MANNED_JAT));
  exactTwenty.pointReleased(0, 1000, {12, 0, 40}, settings);
  exactTwenty.tick(4999);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::JAT_RESULT),
                          static_cast<uint8_t>(exactTwenty.state()));
  exactTwenty.tick(5000);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionState::EDIT_START_TIME),
      static_cast<uint8_t>(exactTwenty.state()));
  TEST_ASSERT_EQUAL_UINT8(1, exactTwenty.proposedStartClockTime().minute);

  domain::CompetitionEngine underTwenty;
  activateAtNoon(underTwenty,
                 jatThenFinishOrder(domain::JatType::MANNED_JAT));
  underTwenty.pointReleased(0, 1000, {12, 0, 41}, settings);
  underTwenty.dismissJatResult(0);
  TEST_ASSERT_EQUAL_UINT8(2, underTwenty.proposedStartClockTime().minute);

  domain::CompetitionEngine midnight;
  activateAtNoon(midnight,
                 jatThenFinishOrder(domain::JatType::MANNED_JAT));
  settings.jatResultSeconds = 0;
  midnight.pointReleased(0, 1000, {23, 59, 50}, settings);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionState::EDIT_START_TIME),
      static_cast<uint8_t>(midnight.state()));
  TEST_ASSERT_EQUAL_UINT8(0, midnight.proposedStartClockTime().hour);
  TEST_ASSERT_EQUAL_UINT8(1, midnight.proposedStartClockTime().minute);
}

void testJatTypesSetCorrectPhysicalDistanceZero() {
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 0;
  domain::CompetitionEngine manned;
  activateAtNoon(manned, jatThenFinishOrder(domain::JatType::MANNED_JAT));
  manned.addDistanceMillimeters(10000);
  manned.pointReleased(1000, 1000, {12, 0, 1}, settings);
  manned.addDistanceMillimeters(5000);
  TEST_ASSERT_EQUAL_INT64(5000, manned.stageDistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(5000, manned.segmentDistanceMillimeters());

  for (domain::JatType type : {domain::JatType::EMIT_MLA,
                               domain::JatType::EMIT_ULA}) {
    domain::CompetitionEngine emit;
    activateAtNoon(emit, jatThenFinishOrder(type));
    emit.addDistanceMillimeters(10000);
    emit.pointReleased(1000, 1000, {12, 0, 1}, settings);
    emit.addDistanceMillimeters(5000);
    TEST_ASSERT_EQUAL_INT64(15000, emit.physicalDistanceMillimeters());
    TEST_ASSERT_EQUAL_INT64(0, emit.stageDistanceMillimeters());
    TEST_ASSERT_TRUE(emit.acceptNextStageStart({12, 1, 0},
                                               12UL * 3600UL * 1000UL + 1000,
                                               1000));
    emit.addDistanceMillimeters(2000);
    TEST_ASSERT_EQUAL_INT64(2000, emit.stageDistanceMillimeters());
    TEST_ASSERT_EQUAL_INT64(2000, emit.segmentDistanceMillimeters());
  }
}

void testJatAcceptedFutureAndPastStartTimes() {
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 0;
  domain::CompetitionEngine future;
  activateAtNoon(future, jatThenFinishOrder(domain::JatType::MANNED_JAT));
  future.pointReleased(0, 1000, {12, 0, 0}, settings);
  TEST_ASSERT_TRUE(future.acceptNextStageStart(
      {12, 1, 0}, 12UL * 3600UL * 1000UL, 0));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::WAIT_START),
                          static_cast<uint8_t>(future.state()));

  domain::CompetitionEngine past;
  activateAtNoon(past, jatThenFinishOrder(domain::JatType::MANNED_JAT));
  past.pointReleased(120000, 1000, {12, 2, 0}, settings);
  TEST_ASSERT_TRUE(past.acceptNextStageStart(
      {12, 1, 0}, (12UL * 3600UL + 120UL) * 1000UL, 120000));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::RUNNING),
                          static_cast<uint8_t>(past.state()));
  TEST_ASSERT_TRUE(past.realTimeMs() > 0);
}

void testAdditionalOrderReplacesActiveSpeedAndPreservesOriginals() {
  domain::RouteOrder order;
  order.startHour = 12;
  order.segments.push_back(segment(0, domain::SegmentType::SPEED, 36));
  order.segments.push_back(segment(1, domain::SegmentType::TIME, 20));
  order.segments.push_back(
      segment(2, domain::SegmentType::TIME, 30, domain::PointType::FINISH_M));
  domain::CompetitionEngine engine;
  activateAtNoon(engine, order);
  engine.addDistanceMillimeters(10000);
  engine.tick(1000);
  TEST_ASSERT_EQUAL_INT64(0, engine.deltaMs());
  domain::SegmentOverride requested;
  requested.overrideType = domain::OverrideType::ADDITIONAL_ORDER;
  requested.startSegmentIndex = 0;
  requested.endPointIndex = 2;
  requested.replacementDurationSeconds = 60;
  domain::SegmentOverride accepted;
  TEST_ASSERT_TRUE(engine.applyOverride(requested, accepted));
  TEST_ASSERT_EQUAL_size_t(2, accepted.originalDefinitions.size());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::SegmentType::SPEED),
                          static_cast<uint8_t>(
                              accepted.originalDefinitions[0].segmentType));
  TEST_ASSERT_EQUAL_INT64(59000, engine.deltaMs());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::ADVANCED),
                          static_cast<uint8_t>(engine.pointReleased(1000)));
  TEST_ASSERT_EQUAL_INT64(59000, engine.deltaMs());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::PointResult::ADVANCED),
                          static_cast<uint8_t>(engine.pointReleased(1000)));
  TEST_ASSERT_NULL(engine.activeOverride());
}

void testOverrideValidationStopsAtJatOrFinishAndTkUsesThreeDurations() {
  domain::RouteOrder order = jatThenFinishOrder(domain::JatType::EMIT_MLA);
  domain::CompetitionEngine engine;
  activateAtNoon(engine, order);
  domain::SegmentOverride request;
  request.overrideType = domain::OverrideType::ADDITIONAL_ORDER;
  request.startSegmentIndex = 0;
  request.endPointIndex = 2;
  request.replacementDurationSeconds = 60;
  domain::SegmentOverride accepted;
  TEST_ASSERT_FALSE(engine.applyOverride(request, accepted));
  request.endPointIndex = 1;
  request.replacementDurationSeconds = 0;
  TEST_ASSERT_FALSE(engine.applyOverride(request, accepted));
  request.overrideType = domain::OverrideType::ROAD_BREAK;
  request.replacementDurationSeconds = 600;
  TEST_ASSERT_FALSE(engine.applyOverride(request, accepted));
  for (uint16_t duration : {static_cast<uint16_t>(660),
                            static_cast<uint16_t>(1260),
                            static_cast<uint16_t>(1860)}) {
    request.replacementDurationSeconds = duration;
    TEST_ASSERT_TRUE(engine.applyOverride(request, accepted));
  }
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

void testMittisAcceptChangesOnlyFuturePulsesAndRejectKeepsOldFactor() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(mittisThenTimeOrder(), true);
  acceptNoon(app);
  app.handleDistancePulses({500, 0, 1000, 1000});
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::MittisProposal),
                          static_cast<uint8_t>(app.screen()));
  press(app, core::ButtonId::Right);
  uint32_t proposed = 0;
  TEST_ASSERT_TRUE(app.takeCalibrationSaveRequest(proposed));
  TEST_ASSERT_EQUAL_UINT32(2000, proposed);
  app.completeCalibrationSave(true);
  TEST_ASSERT_EQUAL_UINT32(2000, app.millimetersPerPulse());
  TEST_ASSERT_EQUAL_INT64(500000, app.trip1DistanceMillimeters());
  app.handleDistancePulses({1, 1000, 2000, 2000});
  TEST_ASSERT_EQUAL_INT64(502000, app.trip1DistanceMillimeters());

  FakeTimeSource rejectSource;
  core::SoftwareClock rejectClock(rejectSource);
  core::ApplicationCore reject(rejectClock, 1000, 1000000);
  reject.setInitialRouteOrder(mittisThenTimeOrder(), true);
  acceptNoon(reject);
  reject.handleDistancePulses({500, 0, 1000, 1000});
  press(reject, core::ButtonId::Point, core::ButtonEventType::Release);
  press(reject, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT32(1000, reject.millimetersPerPulse());
  TEST_ASSERT_EQUAL_UINT16(1, reject.competition().currentSegmentIndex());
}

void testMittisSaveFailureKeepsOldFactorAndPendingProposal() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(mittisThenTimeOrder(), true);
  acceptNoon(app);
  app.handleDistancePulses({500, 0, 1000, 1000});
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  press(app, core::ButtonId::Right);
  uint32_t proposed = 0;
  TEST_ASSERT_TRUE(app.takeCalibrationSaveRequest(proposed));
  app.completeCalibrationSave(false);
  TEST_ASSERT_EQUAL_UINT32(1000, app.millimetersPerPulse());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::MittisProposal),
                          static_cast<uint8_t>(app.screen()));
  TEST_ASSERT_TRUE(app.competition().mittisProposal().pending);
  TEST_ASSERT_TRUE(app.displayModel().mittis.saveFailed);
}

void testMittisResolutionContinuesIntoJatFlow() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  domain::RouteOrder order = mittisThenTimeOrder();
  order.competitionType = domain::CompetitionType::NON_EMIT;
  order.segments[0].pointTypeAtEnd = domain::PointType::JAT;
  order.segments[0].hasJatType = true;
  order.segments[0].jatType = domain::JatType::MANNED_JAT;
  order.segments[0].hasJatOffsetMinutes = true;
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 0;
  app.setCompetitionSettings(settings);
  app.setInitialRouteOrder(order, true);
  acceptNoon(app);
  app.handleDistancePulses({1000, 0, 1000, 1000});
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  press(app, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartTimeEdit),
                          static_cast<uint8_t>(app.screen()));
  TEST_ASSERT_EQUAL_size_t(1, app.competition().stageResults().size());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::START_TIME_PROPOSED),
      static_cast<uint8_t>(app.eventRepository()
                               .at(app.eventRepository().count() - 1)
                               ->eventType));
}

void testMannedJatUsesRightAndPointIsIgnoredDuringStartEdit() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 0;
  app.setCompetitionSettings(settings);
  app.setInitialRouteOrder(jatThenFinishOrder(domain::JatType::MANNED_JAT),
                           true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartTimeEdit),
                          static_cast<uint8_t>(app.screen()));
  TEST_ASSERT_EQUAL_UINT16(1, app.competition().currentSegmentIndex());
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionState::EDIT_START_TIME),
      static_cast<uint8_t>(app.competition().state()));
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::WAIT_START),
                          static_cast<uint8_t>(app.competition().state()));
  TEST_ASSERT_EQUAL_size_t(1, app.competition().stageResults().size());
}

void testEmitOffsetClampsTwoMinutesAndAcceptsOnlyWithPoint() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 0;
  app.setCompetitionSettings(settings);
  app.setInitialRouteOrder(
      jatThenFinishOrder(domain::JatType::EMIT_JAT_OFFSET, 2), true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT8(2, app.displayModel().startTimeEdit.proposedClockTime.minute);
  press(app, core::ButtonId::Up);
  press(app, core::ButtonId::Up);
  press(app, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(4, app.displayModel().startTimeEdit.proposedClockTime.minute);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionState::EDIT_START_TIME),
      static_cast<uint8_t>(app.competition().state()));
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::WAIT_START),
                          static_cast<uint8_t>(app.competition().state()));
}

void testJatResetsTrip1AndMlaUlaResetItAgainWithoutTouchingTrip2() {
  for (domain::JatType type : {domain::JatType::MANNED_JAT,
                               domain::JatType::EMIT_JAT_OFFSET,
                               domain::JatType::EMIT_MLA,
                               domain::JatType::EMIT_ULA}) {
    FakeTimeSource source;
    core::SoftwareClock clock(source);
    core::ApplicationCore app(clock, 1000, 1000000);
    domain::CompetitionSettings settings;
    settings.jatResultSeconds = 0;
    app.setCompetitionSettings(settings);
    app.setInitialRouteOrder(jatThenFinishOrder(type, 2), true);
    acceptNoon(app);
    app.handleDistancePulses({10, 0, 1000, 1000});
    TEST_ASSERT_EQUAL_INT64(10000, app.trip1DistanceMillimeters());
    TEST_ASSERT_EQUAL_INT64(10000, app.trip2DistanceMillimeters());
    press(app, core::ButtonId::Point, core::ButtonEventType::Release);
    TEST_ASSERT_EQUAL_INT64(0, app.trip1DistanceMillimeters());
    TEST_ASSERT_EQUAL_INT64(10000, app.trip2DistanceMillimeters());

    app.handleDistancePulses({5, 1000, 2000, 2000});
    TEST_ASSERT_EQUAL_INT64(5000, app.trip1DistanceMillimeters());
    TEST_ASSERT_EQUAL_INT64(15000, app.trip2DistanceMillimeters());
    if (type == domain::JatType::EMIT_JAT_OFFSET)
      press(app, core::ButtonId::Point, core::ButtonEventType::Release);
    else
      press(app, core::ButtonId::Right);

    if (type == domain::JatType::EMIT_MLA ||
        type == domain::JatType::EMIT_ULA)
      TEST_ASSERT_EQUAL_INT64(0, app.trip1DistanceMillimeters());
    else
      TEST_ASSERT_EQUAL_INT64(5000, app.trip1DistanceMillimeters());
    TEST_ASSERT_EQUAL_INT64(15000, app.trip2DistanceMillimeters());
  }
}

void testJatResultRightSkipsAndTimeoutTransitions() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 5;
  app.setCompetitionSettings(settings);
  app.setInitialRouteOrder(jatThenFinishOrder(domain::JatType::MANNED_JAT),
                           true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::JatResult),
                          static_cast<uint8_t>(app.screen()));
  TEST_ASSERT_TRUE(app.displayModel().competition.deltaFrozen);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartTimeEdit),
                          static_cast<uint8_t>(app.screen()));

  FakeTimeSource timeoutSource;
  core::SoftwareClock timeoutClock(timeoutSource);
  core::ApplicationCore timeout(timeoutClock, 1000, 1000000);
  timeout.setCompetitionSettings(settings);
  timeout.setInitialRouteOrder(
      jatThenFinishOrder(domain::JatType::MANNED_JAT), true);
  acceptNoon(timeout);
  press(timeout, core::ButtonId::Point, core::ButtonEventType::Release);
  timeoutSource.nowMs = 5000;
  timeout.tick(5000000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartTimeEdit),
                          static_cast<uint8_t>(timeout.screen()));
}

void testAcceptedStartCorrectionPreservesPhysicalZeroAndCanCancel() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  domain::CompetitionSettings settings;
  settings.jatResultSeconds = 0;
  app.setCompetitionSettings(settings);
  app.setInitialRouteOrder(jatThenFinishOrder(domain::JatType::MANNED_JAT),
                           true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  press(app, core::ButtonId::Right);
  app.handleDistancePulses({5, 0, 1000, 1000});
  const int64_t stageDistance = app.competition().stageDistanceMillimeters();
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::StartTimeEdit),
                          static_cast<uint8_t>(app.screen()));
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionState::WAIT_START),
                          static_cast<uint8_t>(app.competition().state()));
  TEST_ASSERT_EQUAL_INT64(stageDistance,
                          app.competition().stageDistanceMillimeters());
  press(app, core::ButtonId::Right);
  press(app, core::ButtonId::Up);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_INT64(stageDistance,
                          app.competition().stageDistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::START_TIME_CORRECTED),
      static_cast<uint8_t>(app.eventRepository()
                               .at(app.eventRepository().count() - 1)
                               ->eventType));
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

void testResultMenusShowStageTotalAndOrderedEvents() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::TIME, 10)),
                           true);
  acceptNoon(app);
  source.nowMs = 12000;
  app.tick(12000000);
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_size_t(1, app.competition().stageResults().size());
  const core::DisplayModel finishModel = app.displayModel();
  TEST_ASSERT_TRUE(finishModel.finishResult.visible);
  TEST_ASSERT_EQUAL_UINT8(12, finishModel.finishResult.finishClockTime.hour);
  TEST_ASSERT_EQUAL_UINT8(0, finishModel.finishResult.finishClockTime.minute);
  TEST_ASSERT_EQUAL_UINT8(12, finishModel.finishResult.finishClockTime.second);
  TEST_ASSERT_EQUAL_UINT64(app.competition().totalPoints(),
                           finishModel.finishResult.totalPoints);

  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::MenuPage::Results),
                          static_cast<uint8_t>(app.menuPage()));
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::ResultView),
                          static_cast<uint8_t>(app.screen()));
  TEST_ASSERT_TRUE(app.displayModel().resultView.hasStageResult);
  TEST_ASSERT_EQUAL_UINT64(app.competition().stageResults()[0].points,
                           app.displayModel().resultView.stageResult.points);
  TEST_ASSERT_EQUAL_UINT64(app.competition().stageResults()[0].points,
                           app.displayModel().resultView.totalPoints);
  press(app, core::ButtonId::Left);
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::ResultViewType::TotalPoints),
                          static_cast<uint8_t>(app.displayModel().resultView.type));
  press(app, core::ButtonId::Left);
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Right);
  const core::ResultViewDisplayModel events = app.displayModel().resultView;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::ResultViewType::Events),
                          static_cast<uint8_t>(events.type));
  TEST_ASSERT_TRUE(events.hasEvent);
  TEST_ASSERT_TRUE(events.itemCount >= 3);
}

void testCoreEventAuditIncludesPointUndoReverseTripAndStageResult() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(twoSegmentOrder(domain::SegmentType::TIME, 10,
                                           domain::SegmentType::TIME, 20), true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::Release);
  const size_t pointIndex = app.eventRepository().count() - 1;
  const uint64_t pointId = app.eventRepository().at(pointIndex)->eventId;
  press(app, core::ButtonId::Left);
  TEST_ASSERT_TRUE(app.eventRepository().at(pointIndex)->cancelled);
  const domain::EventRecord* undo =
      app.eventRepository().at(app.eventRepository().count() - 1);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::POINT_UNDO),
      static_cast<uint8_t>(undo->eventType));
  TEST_ASSERT_EQUAL_UINT64(pointId, undo->payload.referencedEventId);
  app.handleReverseSignal({true, 10});
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::REVERSE_CHANGED),
      static_cast<uint8_t>(app.eventRepository()
                               .at(app.eventRepository().count() - 1)
                               ->eventType));
  press(app, core::ButtonId::Trip2Reset);
  const domain::EventRecord* reset =
      app.eventRepository().at(app.eventRepository().count() - 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::DomainEventType::TRIP_RESET),
                          static_cast<uint8_t>(reset->eventType));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::TripChannel::TRIP_2),
                          static_cast<uint8_t>(reset->payload.tripChannel));

  FakeTimeSource finishSource;
  core::SoftwareClock finishClock(finishSource);
  core::ApplicationCore finish(finishClock, 1000, 1000000);
  finish.setInitialRouteOrder(
      orderWith(segment(0, domain::SegmentType::TIME, 10)), true);
  acceptNoon(finish);
  press(finish, core::ButtonId::Point, core::ButtonEventType::Release);
  const std::vector<domain::StageResult> rebuilt =
      domain::stageResultsFromEvents(finish.eventRepository());
  TEST_ASSERT_EQUAL_size_t(1, rebuilt.size());
  TEST_ASSERT_EQUAL_UINT64(finish.competition().totalPoints(),
                           domain::totalStagePoints(rebuilt));
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::OverrideMenu),
                          static_cast<uint8_t>(app.screen()));
  press(app, core::ButtonId::At, core::ButtonEventType::Press);
  TEST_ASSERT_FALSE(app.displayModel().atOverlay.visible);
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  TEST_ASSERT_TRUE(app.displayModel().atOverlay.visible);
  TEST_ASSERT_EQUAL_UINT16(0, app.competition().currentSegmentIndex());
  TEST_ASSERT_EQUAL_INT64(delta, app.competition().deltaMs());
}

void testLongPointOverrideMenuCancelsOrAppliesWithoutPoint() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(twoSegmentOrder(domain::SegmentType::SPEED, 36,
                                           domain::SegmentType::TIME, 20), true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::LongStart);
  TEST_ASSERT_EQUAL_UINT16(0, app.competition().currentSegmentIndex());
  press(app, core::ButtonId::Left);
  TEST_ASSERT_NULL(app.competition().activeOverride());
  TEST_ASSERT_EQUAL_UINT16(0, app.competition().currentSegmentIndex());

  press(app, core::ButtonId::Point, core::ButtonEventType::LongStart);
  press(app, core::ButtonId::Right);  // additional order
  press(app, core::ButtonId::Right);  // start -> duration
  press(app, core::ButtonId::Right);  // duration -> end
  press(app, core::ButtonId::Up);     // through finish point
  press(app, core::ButtonId::Right);  // accept
  TEST_ASSERT_NOT_NULL(app.competition().activeOverride());
  TEST_ASSERT_EQUAL_UINT16(2,
                           app.competition().activeOverride()->endPointIndex);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::ADDITIONAL_ORDER),
      static_cast<uint8_t>(app.eventRepository()
                               .at(app.eventRepository().count() - 1)
                               ->eventType));
}

void testRoadBreakUiCyclesOnlyElevenTwentyOneThirtyOne() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(twoSegmentOrder(domain::SegmentType::TIME, 10,
                                           domain::SegmentType::TIME, 20), true);
  acceptNoon(app);
  press(app, core::ButtonId::Point, core::ButtonEventType::LongStart);
  press(app, core::ButtonId::Down);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT16(660, app.displayModel().overrideEdit.durationSeconds);
  press(app, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT16(1260, app.displayModel().overrideEdit.durationSeconds);
  press(app, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT16(1860, app.displayModel().overrideEdit.durationSeconds);
  press(app, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT16(660, app.displayModel().overrideEdit.durationSeconds);
  press(app, core::ButtonId::Right);
  press(app, core::ButtonId::Right);
  TEST_ASSERT_NOT_NULL(app.competition().activeOverride());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::OverrideType::ROAD_BREAK),
      static_cast<uint8_t>(app.competition().activeOverride()->overrideType));
}

void testAtCreatesTimestampedEventWithoutChangingCompetition() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(twoSegmentOrder(domain::SegmentType::TIME, 10,
                                           domain::SegmentType::TIME, 20), true);
  acceptNoon(app);
  const domain::CompetitionState state = app.competition().state();
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(state),
                          static_cast<uint8_t>(app.competition().state()));
  TEST_ASSERT_EQUAL_UINT16(0, app.competition().currentSegmentIndex());
  const core::DisplayModel model = app.displayModel();
  TEST_ASSERT_TRUE(model.atOverlay.visible);
  TEST_ASSERT_EQUAL_UINT8(12, model.atOverlay.clockTime.hour);
  TEST_ASSERT_EQUAL_UINT8(0, model.atOverlay.clockTime.minute);
  TEST_ASSERT_EQUAL_UINT8(0, model.atOverlay.clockTime.second);
  const domain::EventRecord* event =
      app.eventRepository().at(app.eventRepository().count() - 1);
  TEST_ASSERT_NOT_NULL(event);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::DomainEventType::AT),
                          static_cast<uint8_t>(event->eventType));
}

void testAtSecondPressCancelsWithinWindowAndCreatesNewAfterWindow() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  acceptNoon(app);
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  const size_t firstIndex = app.eventRepository().count() - 1;
  source.nowMs = 3000;
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  TEST_ASSERT_FALSE(app.displayModel().atOverlay.visible);
  TEST_ASSERT_TRUE(app.eventRepository().at(firstIndex)->cancelled);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::AT_CANCELLED),
      static_cast<uint8_t>(app.eventRepository()
                               .at(app.eventRepository().count() - 1)
                               ->eventType));

  source.nowMs = 4000;
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  source.nowMs = 7001;
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  TEST_ASSERT_TRUE(app.displayModel().atOverlay.visible);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::AT),
      static_cast<uint8_t>(app.eventRepository()
                               .at(app.eventRepository().count() - 1)
                               ->eventType));
}

void testAtOverlayRequiresOneSecondStoppedAndUsesAbsoluteTravel() {
  FakeTimeSource source;
  core::SoftwareClock clock(source);
  core::ApplicationCore app(clock, 1000, 1000000);
  app.setInitialRouteOrder(orderWith(segment(0, domain::SegmentType::TIME, 60)),
                           true);
  acceptNoon(app);
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  const size_t atIndex = app.eventRepository().count() - 1;
  source.nowMs = 999;
  app.tick(999000);
  app.handleDistancePulses({10, 0, 1000, 1000000, false});
  TEST_ASSERT_TRUE(app.displayModel().atOverlay.visible);
  TEST_ASSERT_EQUAL_INT64(0,
                          app.displayModel().atOverlay.travelledDistanceMillimeters);
  source.nowMs = 2000;
  app.tick(2000000);
  TEST_ASSERT_FALSE(app.displayModel().atOverlay.waitingForStop);
  app.handleDistancePulses({5, 1000, 2000, 2000000, false});
  app.handleDistancePulses({4, 2000, 3000, 3000000, true});
  TEST_ASSERT_TRUE(app.displayModel().atOverlay.visible);
  TEST_ASSERT_EQUAL_INT64(9000,
                          app.displayModel().atOverlay.travelledDistanceMillimeters);
  app.handleDistancePulses({1, 3000, 4000, 4000000, false});
  TEST_ASSERT_FALSE(app.displayModel().atOverlay.visible);
  TEST_ASSERT_EQUAL_INT64(12000,
                          app.competition().segmentDistanceMillimeters());
  source.nowMs = 2500;
  press(app, core::ButtonId::At, core::ButtonEventType::Release);
  TEST_ASSERT_TRUE(app.eventRepository().at(atIndex)->cancelled);
  TEST_ASSERT_FALSE(app.displayModel().atOverlay.visible);
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
  RUN_TEST(testMittisUsesDurationAndFreezesMeasuredInterval);
  RUN_TEST(testMittisZeroDistanceIsRejectedSafely);
  RUN_TEST(testCalibrationExamplesAndTripIndependence);
  RUN_TEST(testPointResetsOnlySegmentAndCanUndo);
  RUN_TEST(testPointUndoExpires);
  RUN_TEST(testJatCompletesStageAtomically);
  RUN_TEST(testJatResultTimeoutZeroAndMinuteProposalBoundaries);
  RUN_TEST(testJatTypesSetCorrectPhysicalDistanceZero);
  RUN_TEST(testJatAcceptedFutureAndPastStartTimes);
  RUN_TEST(testAdditionalOrderReplacesActiveSpeedAndPreservesOriginals);
  RUN_TEST(testOverrideValidationStopsAtJatOrFinishAndTkUsesThreeDurations);
  RUN_TEST(testFinishFreezesAgainstTimeAndPulses);
  RUN_TEST(testLargeDistanceSaturatesWithoutOverflow);
  RUN_TEST(testActiveStoredOrderStartsAfterClockAcceptance);
  RUN_TEST(testInactiveStoredOrderDoesNotRestart);
  RUN_TEST(testSuccessfulSaveActivatesWaitStartAndFailureDoesNot);
  RUN_TEST(testApplicationPointAndTripResetIntegration);
  RUN_TEST(testMittisAcceptChangesOnlyFuturePulsesAndRejectKeepsOldFactor);
  RUN_TEST(testMittisSaveFailureKeepsOldFactorAndPendingProposal);
  RUN_TEST(testMittisResolutionContinuesIntoJatFlow);
  RUN_TEST(testMannedJatUsesRightAndPointIsIgnoredDuringStartEdit);
  RUN_TEST(testEmitOffsetClampsTwoMinutesAndAcceptsOnlyWithPoint);
  RUN_TEST(testJatResetsTrip1AndMlaUlaResetItAgainWithoutTouchingTrip2);
  RUN_TEST(testJatResultRightSkipsAndTimeoutTransitions);
  RUN_TEST(testAcceptedStartCorrectionPreservesPhysicalZeroAndCanCancel);
  RUN_TEST(testFinishRequestsPersistentCompletion);
  RUN_TEST(testResultMenusShowStageTotalAndOrderedEvents);
  RUN_TEST(testCoreEventAuditIncludesPointUndoReverseTripAndStageResult);
  RUN_TEST(testPulseGeneratorCycleIntegration);
  RUN_TEST(testLilyGoCompetitionGPIOContractAndCoreEvents);
  RUN_TEST(testPointButtonTimingProducesShortOrLongExclusively);
  RUN_TEST(testPointPressLongAndAtDoNotMutateCompetition);
  RUN_TEST(testLongPointOverrideMenuCancelsOrAppliesWithoutPoint);
  RUN_TEST(testRoadBreakUiCyclesOnlyElevenTwentyOneThirtyOne);
  RUN_TEST(testAtCreatesTimestampedEventWithoutChangingCompetition);
  RUN_TEST(testAtSecondPressCancelsWithinWindowAndCreatesNewAfterWindow);
  RUN_TEST(testAtOverlayRequiresOneSecondStoppedAndUsesAbsoluteTravel);
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
