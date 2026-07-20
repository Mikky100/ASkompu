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
#include "route/RouteOrderCodec.h"
#include "route/RouteOrderEditor.h"
#include "route/RouteOrderStore.h"
#include "ui/RouteOrderFormatting.h"

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

class MemoryRouteOrderStore : public route::RouteOrderStore {
 public:
  bool load(domain::RouteOrder& order) override {
    if (!hasValue) return false;
    order = stored;
    return true;
  }
  bool replace(const domain::RouteOrder& order) override {
    if (failReplace || domain::validateRouteOrder(order) !=
                           domain::RouteOrderValidationError::NONE)
      return false;
    stored = order;
    hasValue = true;
    return true;
  }
  bool clear() override {
    hasValue = false;
    return true;
  }
  bool failReplace = false;
  bool hasValue = false;
  domain::RouteOrder stored;
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

domain::RouteOrder makeOrder(domain::CompetitionType competitionType,
                             domain::SegmentType segmentType =
                                 domain::SegmentType::TIME,
                             uint32_t value = 60) {
  domain::RouteOrder order;
  order.competitionType = competitionType;
  domain::SegmentDefinition segment;
  segment.segmentType = segmentType;
  segment.value = value;
  segment.pointTypeAtEnd = domain::PointType::FINISH_M;
  order.segments.push_back(segment);
  return order;
}

void createOneSecondOrder(route::RouteOrderEditor& editor,
                          domain::CompetitionType competitionType) {
  editor.beginCreate();
  if (competitionType == domain::CompetitionType::EMIT)
    editor.handle(route::EditorKey::DOWN);
  editor.handle(route::EditorKey::RIGHT);  // lock competition type
  editor.handle(route::EditorKey::UP);     // start 01:00
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::UP);
  editor.handle(route::EditorKey::UP);     // start 01:02
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);  // TIME -> value
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::UP);     // 0:01
  editor.handle(route::EditorKey::RIGHT);  // continuation
  editor.handle(route::EditorKey::DOWN);
  editor.handle(route::EditorKey::DOWN);   // finish
}

void testEmitAndNonEmitOrderCreation() {
  for (uint8_t kind = 0; kind < 2; ++kind) {
    route::RouteOrderEditor editor;
    const domain::CompetitionType expected =
        kind == 0 ? domain::CompetitionType::EMIT
                  : domain::CompetitionType::NON_EMIT;
    createOneSecondOrder(editor, expected);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(route::EditorResult::SAVE_REQUESTED),
        static_cast<uint8_t>(editor.handle(route::EditorKey::RIGHT)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                            static_cast<uint8_t>(editor.draft().competitionType));
    TEST_ASSERT_EQUAL_UINT8(1, editor.draft().startHour);
    TEST_ASSERT_EQUAL_UINT8(2, editor.draft().startMinute);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(domain::RouteOrderValidationError::NONE),
        static_cast<uint8_t>(domain::validateRouteOrder(editor.draft())));
  }
}

void testCompetitionTypeIsImmutableAfterSelectionAndDuringEdit() {
  route::RouteOrderEditor editor;
  editor.beginCreate();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionType::NON_EMIT),
      static_cast<uint8_t>(editor.draft().competitionType));
  editor.handle(route::EditorKey::UP);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionType::EMIT),
                          static_cast<uint8_t>(editor.draft().competitionType));
  editor.handle(route::EditorKey::DOWN);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::LEFT);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::START_TIME),
      static_cast<uint8_t>(editor.view().phase));
  editor.handle(route::EditorKey::UP);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionType::NON_EMIT),
                          static_cast<uint8_t>(editor.draft().competitionType));

  const domain::RouteOrder current =
      makeOrder(domain::CompetitionType::NON_EMIT);
  editor.beginBrowse(current);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::UP);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionType::NON_EMIT),
      static_cast<uint8_t>(editor.draft().competitionType));
}

void testRouteOrderValidationOrderValuesAndFinish() {
  domain::RouteOrder order = makeOrder(domain::CompetitionType::EMIT);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::NONE),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order.segments[0].value = 0;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INVALID_VALUE),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order = makeOrder(domain::CompetitionType::EMIT,
                    domain::SegmentType::SPEED, 100);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INVALID_VALUE),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order = makeOrder(domain::CompetitionType::EMIT);
  order.startHour = 24;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INVALID_START_TIME),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order = makeOrder(domain::CompetitionType::EMIT);
  order.segments[0].segmentIndex = 1;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INDEX_ORDER),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order = makeOrder(domain::CompetitionType::EMIT);
  order.segments[0].pointTypeAtEnd = domain::PointType::NORMAL;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::MISSING_FINISH),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order.segments[0].pointTypeAtEnd = static_cast<domain::PointType>(99);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INVALID_POINT),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
}

void testJatValidationByCompetitionType() {
  domain::RouteOrder order = makeOrder(domain::CompetitionType::NON_EMIT);
  domain::SegmentDefinition first = order.segments[0];
  first.pointTypeAtEnd = domain::PointType::JAT;
  first.hasJatType = true;
  first.jatType = domain::JatType::MANNED_JAT;
  first.hasJatOffsetMinutes = false;
  order.segments[0] = first;
  domain::SegmentDefinition finish;
  finish.segmentIndex = 1;
  finish.startPointIndex = 1;
  finish.endPointIndex = 2;
  finish.value = 10;
  finish.pointTypeAtEnd = domain::PointType::FINISH_M;
  order.segments.push_back(finish);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::NONE),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order.segments[0].jatType = domain::JatType::EMIT_MLA;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INVALID_JAT),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
}

void testNonEmitJatSkipsOnlyChoiceAndReturnsToContinuation() {
  route::RouteOrderEditor editor;
  editor.beginCreate();
  editor.handle(route::EditorKey::RIGHT);  // NON-EMIT
  editor.handle(route::EditorKey::RIGHT);  // hour -> minute
  editor.handle(route::EditorKey::RIGHT);  // start time
  editor.handle(route::EditorKey::RIGHT);  // TIME value
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::UP);     // 00:01
  editor.handle(route::EditorKey::RIGHT);  // continuation
  editor.handle(route::EditorKey::DOWN);   // JAT
  editor.handle(route::EditorKey::RIGHT);

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::JAT_OFFSET),
      static_cast<uint8_t>(editor.view().phase));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::JatType::MANNED_JAT),
                          static_cast<uint8_t>(editor.view().jatType));

  editor.handle(route::EditorKey::LEFT);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::CONTINUATION),
      static_cast<uint8_t>(editor.view().phase));
}

void testMittisRangeAndForcedFollowingTimeSegment() {
  domain::RouteOrder order;
  domain::SegmentDefinition mittis;
  mittis.segmentType = domain::SegmentType::MITTIS;
  mittis.value = 1000;
  mittis.hasMittisDuration = true;
  mittis.mittisDurationSeconds = 60;
  mittis.pointTypeAtEnd = domain::PointType::FINISH_M;
  order.segments.push_back(mittis);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::NONE),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order.segments[0].value = 999;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INVALID_VALUE),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));
  order.segments[0].value = 1000;
  order.segments[0].hasMittisDuration = false;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::INVALID_MITTIS),
      static_cast<uint8_t>(domain::validateRouteOrder(order)));

  route::RouteOrderEditor editor;
  editor.beginCreate();
  editor.handle(route::EditorKey::RIGHT);  // competition type
  editor.handle(route::EditorKey::RIGHT);  // hour -> minute
  editor.handle(route::EditorKey::RIGHT);  // start time accepted
  editor.handle(route::EditorKey::DOWN);
  editor.handle(route::EditorKey::DOWN);   // MITTIS
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::SegmentType::MITTIS),
                          static_cast<uint8_t>(editor.view().segmentType));
  editor.handle(route::EditorKey::RIGHT);  // distance
  editor.handle(route::EditorKey::UP);     // 1000 m
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);  // accept distance
  TEST_ASSERT_EQUAL_UINT8(0, editor.draft().segments.size());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(route::EditorPhase::VALUE),
                          static_cast<uint8_t>(editor.view().phase));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::SegmentType::MITTIS),
                          static_cast<uint8_t>(editor.view().segmentType));
  TEST_ASSERT_TRUE(editor.view().enteringMittisTime);
  char editableTime[16]{};
  ui::formatEditableRouteOrderValue(editableTime, sizeof(editableTime),
                                    editor.view());
  TEST_ASSERT_NOT_NULL(std::strchr(editableTime, ':'));
  editor.handle(route::EditorKey::LEFT);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(route::EditorPhase::VALUE),
                          static_cast<uint8_t>(editor.view().phase));
  TEST_ASSERT_FALSE(editor.view().enteringMittisTime);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);  // distance accepted again
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::UP);     // 0:01
  editor.handle(route::EditorKey::RIGHT);  // continuation
  editor.handle(route::EditorKey::DOWN);
  editor.handle(route::EditorKey::DOWN);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorResult::SAVE_REQUESTED),
      static_cast<uint8_t>(editor.handle(route::EditorKey::RIGHT)));
  TEST_ASSERT_EQUAL_UINT8(1, editor.draft().segments.size());
  TEST_ASSERT_EQUAL_UINT16(1,
                           editor.draft().segments[0].mittisDurationSeconds);
}

void testEditableRouteOrderValueFormattingKeepsValueKindsSeparate() {
  route::RouteOrderEditorView view{};
  char text[16]{};

  view.segmentType = domain::SegmentType::MITTIS;
  view.value = 1234;
  view.digitCursor = 0;
  ui::formatEditableRouteOrderValue(text, sizeof(text), view);
  TEST_ASSERT_EQUAL_STRING("[1]234", text);

  view.segmentType = domain::SegmentType::SPEED;
  view.value = 42;
  ui::formatEditableRouteOrderValue(text, sizeof(text), view);
  TEST_ASSERT_EQUAL_STRING("[4]2", text);

  view.segmentType = domain::SegmentType::MITTIS;
  view.enteringMittisTime = true;
  view.value = 330;
  view.digitCursor = 2;
  ui::formatEditableRouteOrderValue(text, sizeof(text), view);
  TEST_ASSERT_EQUAL_STRING("05:[3]0", text);
}

void testDriveSegmentFormattingUsesRoutePointNamesAndLabels() {
  char range[16]{};
  domain::SegmentDefinition segment;
  segment.startPointIndex = 0;
  segment.endPointIndex = 1;
  segment.segmentType = domain::SegmentType::TIME;
  ui::formatDriveSegmentRange(range, sizeof(range), segment);
  TEST_ASSERT_EQUAL_STRING("L-1", range);
  TEST_ASSERT_EQUAL_STRING("", ui::driveSegmentLabel(segment));

  segment.segmentType = domain::SegmentType::MITTIS;
  TEST_ASSERT_EQUAL_STRING("MITTIS", ui::driveSegmentLabel(segment));

  segment.startPointIndex = 8;
  segment.endPointIndex = 9;
  segment.segmentType = domain::SegmentType::TIME;
  segment.pointTypeAtEnd = domain::PointType::JAT;
  ui::formatDriveSegmentRange(range, sizeof(range), segment);
  TEST_ASSERT_EQUAL_STRING("8-9", range);
  TEST_ASSERT_EQUAL_STRING("JAT", ui::driveSegmentLabel(segment));

  segment.segmentType = domain::SegmentType::MITTIS;
  segment.pointTypeAtEnd = domain::PointType::FINISH_M;
  ui::formatDriveSegmentRange(range, sizeof(range), segment);
  TEST_ASSERT_EQUAL_STRING("8-M", range);
  TEST_ASSERT_EQUAL_STRING("MAALI", ui::driveSegmentLabel(segment));
}

void testCreationCancelAndValidationFailurePreserveCurrentOrder() {
  Fixture fixture;
  const domain::RouteOrder original =
      makeOrder(domain::CompetitionType::EMIT, domain::SegmentType::TIME, 60);
  fixture.application.setInitialRouteOrder(original);
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);  // access prompt
  press(fixture.application, core::ButtonId::Down);   // edit
  press(fixture.application, core::ButtonId::Right);  // edit
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);  // edit selected
  press(fixture.application, core::ButtonId::Down);   // SPEED, zero draft value
  press(fixture.application, core::ButtonId::Right);  // value
  for (uint8_t i = 0; i < 2; ++i) press(fixture.application, core::ButtonId::Right);
  const domain::RouteOrder* request = nullptr;
  TEST_ASSERT_FALSE(fixture.application.takeRouteOrderSaveRequest(request));
  TEST_ASSERT_EQUAL_UINT32(60,
                           fixture.application.currentRouteOrder()->segments[0].value);
  fixture.application.handleButton(
      button(core::ButtonId::Left, core::ButtonEventType::LongStart));
  TEST_ASSERT_EQUAL_UINT32(60,
                           fixture.application.currentRouteOrder()->segments[0].value);
}

void testRightEditsSegmentLeftCancelsAndFailedSavePreservesCurrent() {
  Fixture fixture;
  const domain::RouteOrder original = makeOrder(domain::CompetitionType::EMIT);
  fixture.application.setInitialRouteOrder(original);
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);  // access prompt
  press(fixture.application, core::ButtonId::Down);   // edit
  press(fixture.application, core::ButtonId::Right);  // edit
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::SEGMENT_TYPE),
      static_cast<uint8_t>(fixture.application.displayModel().order.editor.phase));
  press(fixture.application, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::BROWSE),
      static_cast<uint8_t>(fixture.application.displayModel().order.editor.phase));
  TEST_ASSERT_EQUAL_UINT32(60,
                           fixture.application.currentRouteOrder()->segments[0].value);

  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Down);  // SPEED
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Right);
  const domain::RouteOrder* request = nullptr;
  TEST_ASSERT_TRUE(fixture.application.takeRouteOrderSaveRequest(request));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::SegmentType::SPEED),
                          static_cast<uint8_t>(request->segments[0].segmentType));
  fixture.application.completeRouteOrderSave(false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::SegmentType::TIME),
                          static_cast<uint8_t>(fixture.application
                                                   .currentRouteOrder()
                                                   ->segments[0]
                                                   .segmentType));
}

void testAccessPromptReplacementKeepsOldUntilSave() {
  Fixture fixture;
  const domain::RouteOrder original =
      makeOrder(domain::CompetitionType::NON_EMIT, domain::SegmentType::TIME,
                60);
  fixture.application.setInitialRouteOrder(original);
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);  // access prompt
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::OrderAccessAction::Replace),
      static_cast<uint8_t>(
          fixture.application.displayModel().orderAccess.selectedAction));
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::OrderAccessAction::Edit),
      static_cast<uint8_t>(
          fixture.application.displayModel().orderAccess.selectedAction));
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Right);  // start replacement
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::COMPETITION_TYPE),
      static_cast<uint8_t>(fixture.application.displayModel().order.editor.phase));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionType::NON_EMIT),
      static_cast<uint8_t>(fixture.application.currentRouteOrder()->competitionType));

  press(fixture.application, core::ButtonId::Right);  // lock NON_EMIT
  press(fixture.application, core::ButtonId::Right);  // hour -> minute
  press(fixture.application, core::ButtonId::Right);  // accept 00:00
  press(fixture.application, core::ButtonId::Right);  // TIME value
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Right);  // continuation
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);  // finish/save request

  const domain::RouteOrder* replacement = nullptr;
  TEST_ASSERT_TRUE(
      fixture.application.takeRouteOrderSaveRequest(replacement));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionType::NON_EMIT),
                          static_cast<uint8_t>(replacement->competitionType));
  TEST_ASSERT_EQUAL_UINT32(1, replacement->segments[0].value);
  TEST_ASSERT_EQUAL_UINT32(60,
                           fixture.application.currentRouteOrder()->segments[0].value);
  fixture.application.completeRouteOrderSave(true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::CompetitionType::NON_EMIT),
                          static_cast<uint8_t>(fixture.application
                                                   .currentRouteOrder()
                                                   ->competitionType));
  TEST_ASSERT_EQUAL_UINT32(1,
                           fixture.application.currentRouteOrder()->segments[0].value);
}

void testCancelledReplacementKeepsCurrentOrder() {
  Fixture fixture;
  const domain::RouteOrder original = makeOrder(domain::CompetitionType::EMIT);
  fixture.application.setInitialRouteOrder(original);
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);  // access prompt
  press(fixture.application, core::ButtonId::Right);  // start replacement
  fixture.application.handleButton(
      button(core::ButtonId::Left, core::ButtonEventType::LongStart));
  press(fixture.application, core::ButtonId::Right);  // confirm cancel
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Menu),
                          static_cast<uint8_t>(fixture.application.screen()));
  TEST_ASSERT_EQUAL_UINT32(60,
                           fixture.application.currentRouteOrder()->segments[0].value);
}

void testCodecSaveReloadAndUnknownSchemaHandling() {
  const domain::RouteOrder order =
      makeOrder(domain::CompetitionType::NON_EMIT, domain::SegmentType::SPEED,
                42);
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(route::RouteOrderCodec::encode(order, bytes));
  domain::RouteOrder decoded;
  TEST_ASSERT_TRUE(
      route::RouteOrderCodec::decode(bytes.data(), bytes.size(), decoded));
  TEST_ASSERT_EQUAL_UINT32(42, decoded.segments[0].value);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionType::NON_EMIT),
      static_cast<uint8_t>(decoded.competitionType));
  bytes[4] = 99;
  TEST_ASSERT_FALSE(
      route::RouteOrderCodec::decode(bytes.data(), bytes.size(), decoded));
  domain::RouteOrder old = order;
  old.schemaVersion = 0;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::RouteOrderValidationError::UNSUPPORTED_SCHEMA),
      static_cast<uint8_t>(domain::validateRouteOrder(old)));

  MemoryRouteOrderStore store;
  TEST_ASSERT_TRUE(store.replace(order));
  domain::RouteOrder reloaded;
  TEST_ASSERT_TRUE(store.load(reloaded));
  TEST_ASSERT_EQUAL_UINT32(42, reloaded.segments[0].value);
  store.failReplace = true;
  domain::RouteOrder replacement = order;
  replacement.segments[0].value = 43;
  TEST_ASSERT_FALSE(store.replace(replacement));
  TEST_ASSERT_TRUE(store.load(reloaded));
  TEST_ASSERT_EQUAL_UINT32(42, reloaded.segments[0].value);
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
  TEST_ASSERT_EQUAL_UINT8(6, fixture.application.menuSelectedIndex());
}

void testMainMenuWrapsAndSubmenuClamps() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(6, fixture.application.menuSelectedIndex());
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.application.menuSelectedIndex());
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::OrderEdit),
                          static_cast<uint8_t>(fixture.application.screen()));
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::CompetitionType::EMIT),
      static_cast<uint8_t>(fixture.application.displayModel()
                               .order.editor.competitionType));
}

void testLongMainMenuScrollKeepsSelectionVisible() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  press(fixture.application, core::ButtonId::Up);
  const core::MenuDisplayModel menu = fixture.application.displayModel().menu;
  TEST_ASSERT_EQUAL_UINT8(6, menu.selectedIndex);
  TEST_ASSERT_EQUAL_UINT8(7 - core::MENU_VISIBLE_ROWS, menu.scrollOffset);
  TEST_ASSERT_TRUE(menu.selectedVisibleRow < menu.visibleRowCount);
}

void testOrderMenuStartsUnifiedCreation() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::OrderEdit),
                          static_cast<uint8_t>(fixture.application.screen()));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(route::EditorPhase::COMPETITION_TYPE),
      static_cast<uint8_t>(fixture.application.displayModel().order.editor.phase));
  TEST_ASSERT_EQUAL_STRING(
      "EI EMIT",
      ui::competitionTypeLabel(
          fixture.application.displayModel().order.editor.competitionType));
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_STRING(
      "LAHTOAIKA",
      ui::routeOrderEditorTitle(
          fixture.application.displayModel().order.editor));
}

void testExistingOrderAccessPromptDefaultsToNewAndLeftPreservesOrder() {
  Fixture fixture;
  const domain::RouteOrder original =
      makeOrder(domain::CompetitionType::EMIT, domain::SegmentType::TIME, 60);
  fixture.application.setInitialRouteOrder(original);
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::Screen::OrderAccessPrompt),
      static_cast<uint8_t>(fixture.application.screen()));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(core::OrderAccessAction::Replace),
      static_cast<uint8_t>(
          fixture.application.displayModel().orderAccess.selectedAction));
  press(fixture.application, core::ButtonId::Left);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(core::Screen::Menu),
                          static_cast<uint8_t>(fixture.application.screen()));
  TEST_ASSERT_EQUAL_UINT32(
      60, fixture.application.currentRouteOrder()->segments[0].value);
}

void testOrderBrowseShowsStartTimeBeforeSegments() {
  Fixture fixture;
  domain::RouteOrder order = makeOrder(domain::CompetitionType::EMIT);
  order.startHour = 12;
  order.startMinute = 34;
  fixture.application.setInitialRouteOrder(order);
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);  // access prompt
  press(fixture.application, core::ButtonId::Down);   // edit
  press(fixture.application, core::ButtonId::Right);  // edit
  TEST_ASSERT_TRUE(fixture.application.displayModel().order.showsStartTime);
  TEST_ASSERT_FALSE(
      fixture.application.displayModel().order.hasSelectedSegment);
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_FALSE(fixture.application.displayModel().order.showsStartTime);
  TEST_ASSERT_TRUE(fixture.application.displayModel().order.hasSelectedSegment);
}

void testPersistentTextColorSelectionRequest() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 4);
  press(fixture.application, core::ButtonId::Right);  // display menu
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);  // text color choices
  press(fixture.application, core::ButtonId::Down);   // red
  press(fixture.application, core::ButtonId::Right);
  domain::TextColor requested = domain::TextColor::WHITE;
  TEST_ASSERT_TRUE(fixture.application.takeTextColorSaveRequest(requested));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::TextColor::RED),
                          static_cast<uint8_t>(requested));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::TextColor::WHITE),
      static_cast<uint8_t>(fixture.application.displayModel().textColor));
  fixture.application.completeTextColorSave(true);
  TEST_ASSERT_EQUAL_UINT8(1, fixture.application.menuSelectedIndex());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::TextColor::RED),
      static_cast<uint8_t>(fixture.application.displayModel().textColor));
}

void testMenuTimeEditCancelAndAccept() {
  Fixture fixture;
  acceptStartupTime(fixture, 12, 34);
  openMainAt(fixture, 1);
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
  openMainAt(fixture, 6);
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

void testPulsesContinueInOrderPromptAndEditor() {
  Fixture fixture;
  fixture.application.setInitialRouteOrder(
      makeOrder(domain::CompetitionType::NON_EMIT));
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 0);
  press(fixture.application, core::ButtonId::Right);
  pulse(fixture.application, 1, 0, 100000, 100000);
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);
  pulse(fixture.application, 1, 100000, 200000, 200000);
  TEST_ASSERT_EQUAL_INT64(2000,
                          fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(2, fixture.application.totalPulseCount());
}

void testDiagnosticsContainsStatesAndDoesNotMutateDomain() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  pulse(fixture.application, 10, 900000, 1000000, 1000000);
  const uint64_t trip1 = fixture.application.trip1DistanceMillimeters();
  const uint32_t calibration = fixture.application.millimetersPerPulse();
  openMainAt(fixture, 6);
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
  openMainAt(fixture, 2);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Left);
  uint32_t value = 0;
  TEST_ASSERT_FALSE(fixture.application.takeCalibrationSaveRequest(value));
  TEST_ASSERT_EQUAL_UINT32(1000, fixture.application.millimetersPerPulse());
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Right);  // unchanged accept
  TEST_ASSERT_FALSE(fixture.application.takeCalibrationSaveRequest(value));
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_TRUE(fixture.application.takeCalibrationSaveRequest(value));
  TEST_ASSERT_EQUAL_UINT32(1001, value);
  fixture.application.completeCalibrationSave(true);
  TEST_ASSERT_EQUAL_UINT32(1001, fixture.application.millimetersPerPulse());
}

void testCalibrationLongRepeatUsesTenUnitStep() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 2);
  press(fixture.application, core::ButtonId::Right);
  fixture.application.handleButton(
      button(core::ButtonId::Up, core::ButtonEventType::LongRepeat));
  TEST_ASSERT_EQUAL_UINT32(
      1010,
      fixture.application.displayModel().calibration.editedMillimetersPerPulse);
  fixture.application.handleButton(
      button(core::ButtonId::Down, core::ButtonEventType::LongRepeat));
  TEST_ASSERT_EQUAL_UINT32(
      1000,
      fixture.application.displayModel().calibration.editedMillimetersPerPulse);
}

void testExistingMittisIsNotOfferedForAnotherSegment() {
  domain::RouteOrder order;
  domain::SegmentDefinition mittis;
  mittis.segmentType = domain::SegmentType::MITTIS;
  mittis.value = 1000;
  mittis.hasMittisDuration = true;
  mittis.mittisDurationSeconds = 60;
  order.segments.push_back(mittis);
  domain::SegmentDefinition finish;
  finish.segmentIndex = 1;
  finish.startPointIndex = 1;
  finish.endPointIndex = 2;
  finish.segmentType = domain::SegmentType::TIME;
  finish.value = 60;
  finish.pointTypeAtEnd = domain::PointType::FINISH_M;
  order.segments.push_back(finish);

  route::RouteOrderEditor editor;
  editor.beginBrowse(order);
  editor.handle(route::EditorKey::DOWN);
  editor.handle(route::EditorKey::DOWN);
  editor.handle(route::EditorKey::RIGHT);
  editor.handle(route::EditorKey::DOWN);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::SegmentType::SPEED),
                          static_cast<uint8_t>(editor.view().segmentType));
  editor.handle(route::EditorKey::DOWN);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(domain::SegmentType::TIME),
                          static_cast<uint8_t>(editor.view().segmentType));
}

void testCalibrationSaveFailureStaysInEditor() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openMainAt(fixture, 2);
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

void testFootResetMatchesTrip1ResetWithoutChangingTrip2() {
  Fixture fixture;
  pulse(fixture.application, 7, 600000, 700000, 700000);
  fixture.application.handleButton(
      button(core::ButtonId::FootReset, core::ButtonEventType::Press));
  TEST_ASSERT_EQUAL_UINT64(0, fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(7000,
                           fixture.application.trip2DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT64(7, fixture.application.totalPulseCount());
  fixture.application.handleButton(
      button(core::ButtonId::FootReset, core::ButtonEventType::Release));
}

void testTripMenuActionsResetNamedTripOnly() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  pulse(fixture.application, 5, 400000, 500000, 500000);
  openMainAt(fixture, 5);
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
  TEST_ASSERT_EQUAL_INT64(std::numeric_limits<int64_t>::max(),
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

void testDeferredButtonEdgesPreserveShortPress() {
  input::ButtonInterpreter interpreter(35, 500, 100);
  interpreter.reset(false, 0);

  // Model an edge queue drained after both physical edges have occurred. The
  // previous level is advanced to each interrupt timestamp before applying the
  // new level, just as DebouncedButton does outside the ISR.
  interpreter.update(false, 10);
  interpreter.update(true, 10);
  TEST_ASSERT_TRUE(interpreter.update(true, 110).pressed);
  interpreter.update(false, 110);
  const input::ButtonTransitions released = interpreter.update(false, 210);

  TEST_ASSERT_TRUE(released.released);
  TEST_ASSERT_TRUE(released.shortPress);
  TEST_ASSERT_FALSE(released.longStart);
}

void testButtonInterpreterSuppressesDuplicateBouncePress() {
  input::ButtonInterpreter interpreter(20, 500, 0);
  interpreter.reset(false, 0);
  interpreter.update(true, 0);
  TEST_ASSERT_TRUE(interpreter.update(true, 20).pressed);
  interpreter.update(false, 30);
  TEST_ASSERT_TRUE(interpreter.update(false, 50).shortPress);

  interpreter.update(true, 60);
  const input::ButtonTransitions duplicatePress = interpreter.update(true, 80);
  TEST_ASSERT_FALSE(duplicatePress.pressed);
  interpreter.update(false, 90);
  const input::ButtonTransitions duplicateRelease =
      interpreter.update(false, 110);
  TEST_ASSERT_FALSE(duplicateRelease.shortPress);

  interpreter.update(true, 200);
  TEST_ASSERT_TRUE(interpreter.update(true, 220).pressed);
}

void testButtonTimestampSlightlyBehindCannotStartLongPress() {
  input::ButtonInterpreter point(20, 2000, 0, 500);
  point.reset(false, 900);
  point.update(true, 1000);
  TEST_ASSERT_TRUE(point.update(true, 1020).pressed);

  // ISR tick timestamps can lead millis() by a tick. This must not wrap into
  // an apparent multi-day hold and open the additional-order menu.
  const input::ButtonTransitions behind = point.update(true, 1019);
  TEST_ASSERT_FALSE(behind.longStart);
  TEST_ASSERT_TRUE(point.update(true, 3020).longStart);
}

void testRightButtonGuardRequiresASeparateSlowPress() {
  input::ButtonInterpreter right(20, 500, 0, 350);
  right.reset(false, 0);
  right.update(true, 0);
  TEST_ASSERT_TRUE(right.update(true, 20).pressed);
  right.update(false, 80);
  TEST_ASSERT_TRUE(right.update(false, 100).shortPress);

  right.update(true, 200);
  TEST_ASSERT_FALSE(right.update(true, 220).pressed);
  right.update(false, 260);
  TEST_ASSERT_FALSE(right.update(false, 280).shortPress);

  right.update(true, 370);
  TEST_ASSERT_TRUE(right.update(true, 390).pressed);
  right.update(false, 450);
  TEST_ASSERT_TRUE(right.update(false, 470).shortPress);
}

void testMittisStartResetsTripAndFreezesDisplayedTripForTenSeconds() {
  Fixture fixture;
  domain::RouteOrder order;
  order.startHour = 0;
  order.startMinute = 0;
  domain::SegmentDefinition mittis;
  mittis.segmentType = domain::SegmentType::MITTIS;
  mittis.value = 1000;
  mittis.hasMittisDuration = true;
  mittis.mittisDurationSeconds = 60;
  mittis.pointTypeAtEnd = domain::PointType::FINISH_M;
  order.segments.push_back(mittis);
  fixture.application.setInitialRouteOrder(order, true);
  acceptStartupTime(fixture, 0, 0);
  fixture.application.tick(0);

  pulse(fixture.application, 2, 100000, 200000, 200000);
  TEST_ASSERT_EQUAL_INT64(2000,
                          fixture.application.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_INT64(
      0, fixture.application.displayModel().trip1.distanceMillimeters);

  fixture.timeSource.advance(9999);
  fixture.application.tick(9999000);
  TEST_ASSERT_EQUAL_INT64(
      0, fixture.application.displayModel().trip1.distanceMillimeters);
  fixture.timeSource.advance(1);
  fixture.application.tick(10000000);
  TEST_ASSERT_EQUAL_INT64(
      2000, fixture.application.displayModel().trip1.distanceMillimeters);
}

void openDebugMenu(Fixture& fixture) {
  openMainAt(fixture, 6);
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);
}

void testDebugSpeedDefaultsOffAndUnknownBitsAreFiltered() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  TEST_ASSERT_FALSE(fixture.application.displayModel().showSpeed);
  fixture.application.setInitialDebugDisplaySettings(
      domain::DebugDisplaySettings(0xFFFF));
  TEST_ASSERT_TRUE(fixture.application.displayModel().showSpeed);
  const domain::DebugDisplaySettings validated =
      domain::validatedDebugDisplaySettings(
          domain::DebugDisplaySettings(0xFFFE));
  TEST_ASSERT_EQUAL_UINT16(0, validated.enabledElements);
}

void testDebugSpeedCanBeEnabledDisabledAndFailureKeepsOldValue() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openDebugMenu(fixture);
  TEST_ASSERT_EQUAL_STRING("NOPEUS: POIS",
                           fixture.application.displayModel().menu.rows[0].label);
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_STRING("NOPEUS: PAALLA",
                           fixture.application.displayModel().menu.rows[0].label);
  TEST_ASSERT_FALSE(fixture.application.displayModel().showSpeed);
  press(fixture.application, core::ButtonId::Right);
  domain::DebugDisplaySettings requested;
  TEST_ASSERT_TRUE(
      fixture.application.takeDebugDisplaySettingsSaveRequest(requested));
  TEST_ASSERT_TRUE(requested.enabled(domain::DebugDisplayElement::SPEED));
  TEST_ASSERT_FALSE(
      fixture.application.takeDebugDisplaySettingsSaveRequest(requested));
  fixture.application.completeDebugDisplaySettingsSave(false);
  TEST_ASSERT_FALSE(fixture.application.displayModel().showSpeed);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_STRING("NOPEUS: POIS",
                           fixture.application.displayModel().menu.rows[0].label);

  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_TRUE(
      fixture.application.takeDebugDisplaySettingsSaveRequest(requested));
  fixture.application.completeDebugDisplaySettingsSave(true);
  TEST_ASSERT_TRUE(fixture.application.displayModel().showSpeed);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_STRING(
      "NOPEUS: PAALLA", fixture.application.displayModel().menu.rows[0].label);

  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_TRUE(
      fixture.application.takeDebugDisplaySettingsSaveRequest(requested));
  fixture.application.completeDebugDisplaySettingsSave(true);
  TEST_ASSERT_FALSE(fixture.application.displayModel().showSpeed);
}

void testUnchangedDebugSettingDoesNotRequestPersistentWrite() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  openDebugMenu(fixture);
  domain::DebugDisplaySettings requested;
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_FALSE(
      fixture.application.takeDebugDisplaySettingsSaveRequest(requested));
  press(fixture.application, core::ButtonId::Right);
  press(fixture.application, core::ButtonId::Up);
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_FALSE(
      fixture.application.takeDebugDisplaySettingsSaveRequest(requested));
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_EQUAL_STRING("NOPEUS: POIS",
                           fixture.application.displayModel().menu.rows[0].label);
}

void testDisplayBrightnessAndLabelsAreEditedThenAccepted() {
  Fixture fixture;
  acceptStartupTime(fixture, 0, 0);
  fixture.application.setInitialDisplaySettings({60, true});
  openMainAt(fixture, 4);
  press(fixture.application, core::ButtonId::Right);

  press(fixture.application, core::ButtonId::Right);  // brightness
  TEST_ASSERT_EQUAL_STRING("KIRKKAUS: 60%",
                           fixture.application.displayModel().menu.rows[0].label);
  press(fixture.application, core::ButtonId::Up);
  TEST_ASSERT_EQUAL_UINT8(70,
                          fixture.application.displayModel().backlightPercent);
  domain::DisplaySettings requested;
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_TRUE(
      fixture.application.takeDisplaySettingsSaveRequest(requested));
  TEST_ASSERT_EQUAL_UINT8(70, requested.backlightPercent);
  TEST_ASSERT_TRUE(requested.showLabels);
  fixture.application.completeDisplaySettingsSave(true);

  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Down);
  press(fixture.application, core::ButtonId::Right);  // labels
  press(fixture.application, core::ButtonId::Down);
  TEST_ASSERT_EQUAL_STRING("SELITTEET: POIS",
                           fixture.application.displayModel().menu.rows[0].label);
  press(fixture.application, core::ButtonId::Right);
  TEST_ASSERT_TRUE(
      fixture.application.takeDisplaySettingsSaveRequest(requested));
  TEST_ASSERT_EQUAL_UINT8(70, requested.backlightPercent);
  TEST_ASSERT_FALSE(requested.showLabels);
  fixture.application.completeDisplaySettingsSave(true);
  TEST_ASSERT_EQUAL_UINT8(70,
                          fixture.application.displayModel().backlightPercent);
  TEST_ASSERT_FALSE(fixture.application.displayModel().showLabels);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testEmitAndNonEmitOrderCreation);
  RUN_TEST(testCompetitionTypeIsImmutableAfterSelectionAndDuringEdit);
  RUN_TEST(testRouteOrderValidationOrderValuesAndFinish);
  RUN_TEST(testJatValidationByCompetitionType);
  RUN_TEST(testNonEmitJatSkipsOnlyChoiceAndReturnsToContinuation);
  RUN_TEST(testMittisRangeAndForcedFollowingTimeSegment);
  RUN_TEST(testEditableRouteOrderValueFormattingKeepsValueKindsSeparate);
  RUN_TEST(testDriveSegmentFormattingUsesRoutePointNamesAndLabels);
  RUN_TEST(testCreationCancelAndValidationFailurePreserveCurrentOrder);
  RUN_TEST(testRightEditsSegmentLeftCancelsAndFailedSavePreservesCurrent);
  RUN_TEST(testAccessPromptReplacementKeepsOldUntilSave);
  RUN_TEST(testCancelledReplacementKeepsCurrentOrder);
  RUN_TEST(testCodecSaveReloadAndUnknownSchemaHandling);
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
  RUN_TEST(testOrderMenuStartsUnifiedCreation);
  RUN_TEST(testExistingOrderAccessPromptDefaultsToNewAndLeftPreservesOrder);
  RUN_TEST(testOrderBrowseShowsStartTimeBeforeSegments);
  RUN_TEST(testPersistentTextColorSelectionRequest);
  RUN_TEST(testMenuTimeEditCancelAndAccept);
  RUN_TEST(testClockAndPulsesContinueInMenuAndDiagnostics);
  RUN_TEST(testPulsesContinueInOrderPromptAndEditor);
  RUN_TEST(testDiagnosticsContainsStatesAndDoesNotMutateDomain);
  RUN_TEST(testCalibrationValidationStepAndFallback);
  RUN_TEST(testCalibrationMenuCancelUnchangedAndChangedSave);
  RUN_TEST(testCalibrationLongRepeatUsesTenUnitStep);
  RUN_TEST(testExistingMittisIsNotOfferedForAnotherSegment);
  RUN_TEST(testCalibrationSaveFailureStaysInEditor);
  RUN_TEST(testTripResetsAreIndependentAndKeepTotal);
  RUN_TEST(testFootResetMatchesTrip1ResetWithoutChangingTrip2);
  RUN_TEST(testTripMenuActionsResetNamedTripOnly);
  RUN_TEST(testPulsesAccumulateDuringStartupTimeEntry);
  RUN_TEST(testSparseAndDensePulseSpeeds);
  RUN_TEST(testSpeedReturnsToZeroAndTimestampRollover);
  RUN_TEST(testGeneratorCycleDistanceSpeedAndPauses);
  RUN_TEST(testCalibrationChangeAffectsOnlyFuturePulses);
  RUN_TEST(testDistanceMathAndLongRunSaturate);
  RUN_TEST(testButtonDebounceAndLongRepeatSemantics);
  RUN_TEST(testDeferredButtonEdgesPreserveShortPress);
  RUN_TEST(testButtonInterpreterSuppressesDuplicateBouncePress);
  RUN_TEST(testButtonTimestampSlightlyBehindCannotStartLongPress);
  RUN_TEST(testRightButtonGuardRequiresASeparateSlowPress);
  RUN_TEST(testMittisStartResetsTripAndFreezesDisplayedTripForTenSeconds);
  RUN_TEST(testDebugSpeedDefaultsOffAndUnknownBitsAreFiltered);
  RUN_TEST(testDebugSpeedCanBeEnabledDisabledAndFailureKeepsOldValue);
  RUN_TEST(testUnchangedDebugSettingDoesNotRequestPersistentWrite);
  RUN_TEST(testDisplayBrightnessAndLabelsAreEditedThenAccepted);
  return UNITY_END();
}
