#include <unity.h>
#include <cstring>
#include <limits>
#include "core/ApplicationCore.h"
#include "domain/TripPrecision.h"
#include "input/PulseDiagnostics.h"
#include "ui/TripFormatting.h"

void setUp() {}
void tearDown() {}
namespace {
class Time : public core::TimeSource {
 public:
  uint32_t monotonicMilliseconds() const override { return ms; }
  uint32_t ms = 0;
};
struct Fixture {
  Time time;
  core::SoftwareClock clock{time};
  core::ApplicationCore app{clock, 212, 1000000};
  void press(core::ButtonId id) { app.handleButton({id, core::ButtonEventType::Press, time.ms}); }
  void start() { press(core::ButtonId::Right); press(core::ButtonId::Right); }
  void buttons() {
    press(core::ButtonId::Up);  // last main item: System
    press(core::ButtonId::Right);
    press(core::ButtonId::Down); press(core::ButtonId::Down);
    press(core::ButtonId::Right);
  }
};
domain::RouteOrder jatOrder(domain::JatType type) {
  domain::RouteOrder order;
  order.competitionType = domain::CompetitionType::EMIT;
  order.startHour = 11; order.startMinute = 20;
  domain::SegmentDefinition s;
  s.segmentIndex = s.startPointIndex = 0; s.endPointIndex = 1;
  s.segmentType = domain::SegmentType::TIME; s.value = 214;
  s.pointTypeAtEnd = domain::PointType::JAT; s.hasJatType = true; s.jatType = type;
  order.segments.push_back(s);
  s.segmentIndex = s.startPointIndex = 1; s.endPointIndex = 2;
  s.pointTypeAtEnd = domain::PointType::FINISH_M; s.hasJatType = false; s.value = 300;
  order.segments.push_back(s);
  return order;
}
void testMlaUlaTimeIsAddedAtStartExactlyOnce() {
  for (auto type : {domain::JatType::EMIT_MLA, domain::JatType::EMIT_ULA}) {
    domain::CompetitionEngine engine;
    TEST_ASSERT_TRUE(engine.activate(jatOrder(type), (11*3600+20*60)*1000UL, 0));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(domain::PointResult::JAT_COMPLETED),
        static_cast<int>(engine.pointReleased(214000, 212, {11,23,34}, {})));
    engine.dismissJatResult(214000);
    TEST_ASSERT_TRUE(engine.acceptNextStageStart({11,25,0}, (11*3600+23*60+34)*1000UL, 214000));
    for (uint64_t ms : {214000ULL, 250000ULL, 299999ULL}) {
      engine.tick(ms); engine.tick(ms);
      TEST_ASSERT_EQUAL_INT64(0, engine.idealTimeMs());
      TEST_ASSERT_EQUAL_INT64(300000-ms, engine.deltaMs());
    }
    engine.tick(300000); engine.tick(300000);
    TEST_ASSERT_EQUAL_INT64(300000, engine.idealTimeMs());
    TEST_ASSERT_EQUAL_INT64(300000, engine.deltaMs());
    engine.tick(600000); engine.tick(600000);
    TEST_ASSERT_EQUAL_INT64(0, engine.deltaMs()); // 11:30
  }
}
void testJatCorrectionMidnightAndPastStart() {
  auto order = jatOrder(domain::JatType::EMIT_ULA);
  order.startHour = 23; order.startMinute = 58;
  domain::CompetitionEngine engine;
  TEST_ASSERT_TRUE(engine.activate(order, (23*3600+58*60)*1000UL, 0));
  engine.pointReleased(60000, 212, {23,59,0}, {});
  engine.dismissJatResult(60000);
  TEST_ASSERT_TRUE(engine.acceptNextStageStart({0,1,0}, (23*3600+59*60)*1000UL, 60000));
  TEST_ASSERT_EQUAL_INT64(0, engine.idealTimeMs());
  TEST_ASSERT_TRUE(engine.beginStartTimeCorrection());
  engine.rejectStartTimeCorrection(90000);
  TEST_ASSERT_EQUAL_INT64(0, engine.idealTimeMs());
  TEST_ASSERT_TRUE(engine.beginStartTimeCorrection());
  TEST_ASSERT_TRUE(engine.acceptNextStageStart({23,58,0}, (23*3600+59*60+30)*1000UL, 90000));
  TEST_ASSERT_EQUAL_INT64(300000, engine.idealTimeMs());
  TEST_ASSERT_EQUAL_INT64(210000, engine.deltaMs());
}
void testPrecisionContinuousThresholdsAndRollover() {
  domain::TripPrecision p;
  TEST_ASSERT_FALSE(p.tenMeters());
  p.update(32, 0); p.update(32, 1499); TEST_ASSERT_FALSE(p.tenMeters());
  p.update(31, 1500); p.update(32, 2000); p.update(32,3499);
  TEST_ASSERT_FALSE(p.tenMeters()); p.update(32,3500); TEST_ASSERT_TRUE(p.tenMeters());
  p.update(29,4000); p.update(31,6000); TEST_ASSERT_TRUE(p.tenMeters());
  p.update(28,7000); p.update(30,8499); p.update(28,9000);
  p.update(28,10499); TEST_ASSERT_TRUE(p.tenMeters());
  p.update(28,10500); TEST_ASSERT_FALSE(p.tenMeters());
  p.update(32,UINT32_MAX-999); p.update(32,499); TEST_ASSERT_FALSE(p.tenMeters());
  p.update(32,500); TEST_ASSERT_TRUE(p.tenMeters());
}
void testTripFormattingSignedBoundariesAndLargeValues() {
  char text[40];
  for (bool coarse : {false,true}) {
    ui::formatTrip(text,sizeof(text),2343000,coarse);
    TEST_ASSERT_EQUAL_STRING(coarse ? "2.340" : "2.343",text);
    ui::formatTrip(text,sizeof(text),-2349000,coarse);
    TEST_ASSERT_EQUAL_STRING(coarse ? "-2.340" : "-2.349",text);
    ui::formatTrip(text,sizeof(text),2350000,coarse); TEST_ASSERT_EQUAL_STRING("2.350",text);
    ui::formatTrip(text,sizeof(text),9999999,coarse);
    TEST_ASSERT_EQUAL_STRING(coarse ? "9.990" : "9.999",text);
    ui::formatTrip(text,sizeof(text),10000000,coarse); TEST_ASSERT_EQUAL_STRING("10.00",text);
    ui::formatTrip(text,sizeof(text),-12349999,coarse); TEST_ASSERT_EQUAL_STRING("-12.34",text);
    ui::formatTrip(text,sizeof(text),std::numeric_limits<int64_t>::min(),coarse);
    TEST_ASSERT_EQUAL_STRING("-9223372036854.77",text);
  }
}
void testDisplayPowerIsSessionOnlyAndWakePreservesCalculation() {
  Fixture f; TEST_ASSERT_TRUE(f.app.displayOn());
  f.app.handleLightSwitch(false); TEST_ASSERT_TRUE(f.app.displayOn());
  f.start(); f.buttons();
  TEST_ASSERT_EQUAL_STRING("VIRTAPAINIKE: POIS",f.app.displayModel().menu.rows[0].label);
  f.press(core::ButtonId::Right);
  TEST_ASSERT_EQUAL_STRING("VIRTAPAINIKE: PAALLE",f.app.displayModel().menu.rows[0].label);
  f.app.handleLightSwitch(false); TEST_ASSERT_FALSE(f.app.displayOn());
  f.app.wakeDisplay(); TEST_ASSERT_TRUE(f.app.displayOn());
  f.press(core::ButtonId::Down); f.press(core::ButtonId::Right);
  TEST_ASSERT_FALSE(f.app.displayOn());
  f.app.handleDistancePulses({10,90000,100000,100000});
  f.time.ms=1000; f.app.tick(1000000);
  TEST_ASSERT_EQUAL_INT64(2120,f.app.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT8(1,f.clock.now().second);
  f.press(core::ButtonId::Trip2Reset); TEST_ASSERT_TRUE(f.app.displayOn());
  TEST_ASSERT_EQUAL_INT64(2120,f.app.trip2DistanceMillimeters()); // default external target is Trip1
  Fixture reboot; TEST_ASSERT_TRUE(reboot.app.displayOn());
  reboot.app.handleLightSwitch(false); TEST_ASSERT_TRUE(reboot.app.displayOn());
  reboot.start(); reboot.buttons();
  reboot.press(core::ButtonId::Down); reboot.press(core::ButtonId::Right);
  TEST_ASSERT_FALSE(reboot.app.displayOn()); // menu works with power switch disabled
  for (auto id : {core::ButtonId::Left,core::ButtonId::Right,core::ButtonId::Up,
                 core::ButtonId::Down,core::ButtonId::Point,core::ButtonId::At,
                 core::ButtonId::Trip1Reset,core::ButtonId::Trip2Reset,core::ButtonId::FootReset}) {
    reboot.app.handleButton({id,core::ButtonEventType::Release,0});
    TEST_ASSERT_TRUE(reboot.app.displayOn());
  }
}
void testPulseCountersRejectShortEdgesAndCapturePrehistoryAcrossWrap() {
  input::PulseDiagnostics d;
  uint32_t last=UINT32_MAX-20000;
  d.observe(last,true,false,0); d.delivered(1);
  for (int i=0;i<40;++i) { uint32_t now=last+10600; d.observe(now,true,true,last); last=now; d.delivered(1); }
  d.observe(last+1,false,true,last);
  auto s=d.snapshot(3816);
  TEST_ASSERT_EQUAL_UINT32(42,s.observed); TEST_ASSERT_EQUAL_UINT32(41,s.accepted);
  TEST_ASSERT_EQUAL_UINT32(1,s.rejected); TEST_ASSERT_EQUAL_UINT32(41,s.delivered);
  TEST_ASSERT_EQUAL_UINT32(1,s.observedIntervalUs);
  TEST_ASSERT_EQUAL_UINT32(10600,s.acceptedIntervalUs);
  TEST_ASSERT_EQUAL_UINT32(10600,s.minimumAcceptedUs);
  d.observe(last+21200,true,true,last); d.delivered(1);
  s=d.snapshot(3816); TEST_ASSERT_TRUE(s.frozen); TEST_ASSERT_TRUE(s.automaticTrigger);
  TEST_ASSERT_EQUAL_UINT32(21199,s.observedIntervalUs);
  TEST_ASSERT_EQUAL_UINT32(21200,s.maximumAcceptedUs);
  TEST_ASSERT_EQUAL_UINT8(32,s.historySize); TEST_ASSERT_TRUE(s.historyOverwrites>0);
  TEST_ASSERT_FALSE(d.trace(30).accepted); TEST_ASSERT_TRUE(d.trace(31).accepted);
  TEST_ASSERT_EQUAL_UINT32(21200,d.trace(31).sinceAcceptedUs);
  d.observe(last+31800,true,true,last+21200);
  TEST_ASSERT_EQUAL_UINT32(1,d.snapshot(3816).historyNotRecorded);
  d.reset(); s=d.snapshot(3816);
  TEST_ASSERT_EQUAL_UINT32(0,s.observed); TEST_ASSERT_FALSE(s.frozen);
  d.delivered(UINT32_MAX); d.delivered(1); TEST_ASSERT_TRUE(d.snapshot(3816).counterOverflow);
  d.pendingOverflow(); d.lifetimeOverflow();
  TEST_ASSERT_EQUAL_UINT32(1,d.snapshot(3816).pendingOverflows);
  TEST_ASSERT_EQUAL_UINT32(1,d.snapshot(3816).lifetimeOverflows);
}
void testTenThousandPulsesAtSeveralFrequenciesAndResetIsolation() {
  const uint32_t min=input::minimumPulseIntervalUs(212,200);
  TEST_ASSERT_EQUAL_UINT32(3816,min);
  for (uint32_t period : {50000UL,10600UL,6000UL,3816UL}) {
    Fixture f; input::PulseDiagnostics d;
    uint32_t last=0; bool has=false;
    for (uint32_t i=1;i<=10000;++i) {
      const uint32_t now=i*period;
      TEST_ASSERT_TRUE(input::acceptsPulseInterval(now,last,min,has));
      d.observe(now,true,has,last);
      f.app.handleDistancePulses({1,last,now,now}); d.delivered(1);
      // Simulate main-loop display-model and diagnostic reads between edges.
      f.app.tick(now); f.app.displayModel(); d.snapshot(min);
      d.observe(now+1,false,true,now);
      TEST_ASSERT_FALSE(input::acceptsPulseInterval(now+1,now,min,true));
      last=now; has=true;
    }
    auto s=d.snapshot(min);
    TEST_ASSERT_EQUAL_UINT32(20000,s.observed); TEST_ASSERT_EQUAL_UINT32(10000,s.accepted);
    TEST_ASSERT_EQUAL_UINT32(10000,s.rejected); TEST_ASSERT_EQUAL_UINT32(10000,s.delivered);
    TEST_ASSERT_EQUAL_INT64(2120000,f.app.trip1DistanceMillimeters());
    TEST_ASSERT_EQUAL_INT64(2120000,f.app.trip2DistanceMillimeters());
    d.reset();
    TEST_ASSERT_EQUAL_INT64(2120000,f.app.trip1DistanceMillimeters());
    TEST_ASSERT_EQUAL_UINT32(212,f.app.millimetersPerPulse());
  }
}
void testPulseDiagnosticsMenuRequestsDoNotResetTripsOrSettings() {
  Fixture f; f.start(); f.app.handleDistancePulses({10,0,10600,10600});
  f.press(core::ButtonId::Up); f.press(core::ButtonId::Right);
  f.press(core::ButtonId::Right); f.press(core::ButtonId::Right);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(core::Screen::PulseDiagnostics),static_cast<int>(f.app.screen()));
  f.press(core::ButtonId::Down); TEST_ASSERT_TRUE(f.app.takePulseDiagnosticsResetRequest());
  TEST_ASSERT_FALSE(f.app.takePulseDiagnosticsResetRequest());
  f.press(core::ButtonId::Right); TEST_ASSERT_TRUE(f.app.takePulseDiagnosticsDumpRequest());
  TEST_ASSERT_EQUAL_INT64(2120,f.app.trip1DistanceMillimeters());
  TEST_ASSERT_EQUAL_UINT32(212,f.app.millimetersPerPulse());
}
}
int main(int,char**) {
  UNITY_BEGIN();
  RUN_TEST(testMlaUlaTimeIsAddedAtStartExactlyOnce);
  RUN_TEST(testJatCorrectionMidnightAndPastStart);
  RUN_TEST(testPrecisionContinuousThresholdsAndRollover);
  RUN_TEST(testTripFormattingSignedBoundariesAndLargeValues);
  RUN_TEST(testDisplayPowerIsSessionOnlyAndWakePreservesCalculation);
  RUN_TEST(testPulseCountersRejectShortEdgesAndCapturePrehistoryAcrossWrap);
  RUN_TEST(testTenThousandPulsesAtSeveralFrequenciesAndResetIsolation);
  RUN_TEST(testPulseDiagnosticsMenuRequestsDoNotResetTripsOrSettings);
  return UNITY_END();
}
