#include <unity.h>

#include <cstdint>
#include <limits>

#include "domain/CompetitionRecords.h"
#include "domain/EventRepository.h"

void setUp() {}
void tearDown() {}

namespace {

void testScoringTruncatesTowardZeroAndUsesFactors() {
  const domain::EventClockTime clock{12, 34, 56};
  domain::StageResult early = domain::scoreStage(
      0, domain::PointType::JAT, clock, 1999, 1, 3);
  TEST_ASSERT_EQUAL_INT64(1, early.finalDeltaSeconds);
  TEST_ASSERT_EQUAL_UINT64(0, early.lateSeconds);
  TEST_ASSERT_EQUAL_UINT64(1, early.earlySeconds);
  TEST_ASSERT_EQUAL_UINT64(3, early.points);

  domain::StageResult late = domain::scoreStage(
      1, domain::PointType::FINISH_M, clock, -1999, 1, 3);
  TEST_ASSERT_EQUAL_INT64(-1, late.finalDeltaSeconds);
  TEST_ASSERT_EQUAL_UINT64(1, late.lateSeconds);
  TEST_ASSERT_EQUAL_UINT64(0, late.earlySeconds);
  TEST_ASSERT_EQUAL_UINT64(1, late.points);

  domain::StageResult subSecond = domain::scoreStage(
      2, domain::PointType::JAT, clock, -999, 1, 3);
  TEST_ASSERT_EQUAL_INT64(0, subSecond.finalDeltaSeconds);
  TEST_ASSERT_EQUAL_UINT64(0, subSecond.points);
}

void testScoringAndTotalsSaturate() {
  const domain::StageResult huge = domain::scoreStage(
      0, domain::PointType::FINISH_M, {},
      std::numeric_limits<int64_t>::max(), 1,
      std::numeric_limits<uint16_t>::max());
  TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(), huge.points);
  std::vector<domain::StageResult> results(2, huge);
  TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                           domain::totalStagePoints(results));
}

void testRamRepositoryAssignsOrderedIdsAndMarksCancellation() {
  domain::RamEventRepository repository;
  domain::EventRecord first;
  first.eventType = domain::DomainEventType::NORMAL_POINT;
  domain::EventRecord second;
  second.eventType = domain::DomainEventType::AT;
  uint64_t firstId = 0;
  uint64_t secondId = 0;
  TEST_ASSERT_TRUE(repository.append(first, firstId));
  TEST_ASSERT_TRUE(repository.append(second, secondId));
  TEST_ASSERT_EQUAL_UINT64(1, firstId);
  TEST_ASSERT_EQUAL_UINT64(2, secondId);
  TEST_ASSERT_EQUAL_size_t(2, repository.count());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(domain::DomainEventType::NORMAL_POINT),
      static_cast<uint8_t>(repository.at(0)->eventType));
  TEST_ASSERT_TRUE(repository.markCancelled(firstId));
  TEST_ASSERT_TRUE(repository.at(0)->cancelled);
  TEST_ASSERT_FALSE(repository.at(1)->cancelled);
}

void testStageResultsCanBeRebuiltFromEvents() {
  domain::RamEventRepository repository;
  domain::EventRecord jat;
  jat.eventType = domain::DomainEventType::JAT;
  jat.payload.hasStageResult = true;
  jat.payload.stageResult =
      domain::scoreStage(0, domain::PointType::JAT, {}, -4000, 1, 3);
  uint64_t id = 0;
  TEST_ASSERT_TRUE(repository.append(jat, id));
  const std::vector<domain::StageResult> rebuilt =
      domain::stageResultsFromEvents(repository);
  TEST_ASSERT_EQUAL_size_t(1, rebuilt.size());
  TEST_ASSERT_EQUAL_UINT64(4, rebuilt[0].points);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testScoringTruncatesTowardZeroAndUsesFactors);
  RUN_TEST(testScoringAndTotalsSaturate);
  RUN_TEST(testRamRepositoryAssignsOrderedIdsAndMarksCancellation);
  RUN_TEST(testStageResultsCanBeRebuiltFromEvents);
  return UNITY_END();
}
