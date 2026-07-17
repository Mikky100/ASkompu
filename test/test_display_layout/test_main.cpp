#include <unity.h>

#include <initializer_list>

#include "ui/DisplayLayout.h"

void setUp() {}
void tearDown() {}

namespace {

void assertAllInside(const ui::DisplayLayout& layout) {
  TEST_ASSERT_TRUE(ui::isInside(layout.trip1, layout.width, layout.height));
  TEST_ASSERT_TRUE(ui::isInside(layout.clock, layout.width, layout.height));
  TEST_ASSERT_TRUE(ui::isInside(layout.trip2, layout.width, layout.height));
  TEST_ASSERT_TRUE(
      ui::isInside(layout.currentSegment, layout.width, layout.height));
  TEST_ASSERT_TRUE(ui::isInside(layout.delta, layout.width, layout.height));
  TEST_ASSERT_TRUE(
      ui::isInside(layout.nextSegment, layout.width, layout.height));
  TEST_ASSERT_TRUE(
      ui::isInside(layout.debugSpeed, layout.width, layout.height));
}

void testLilygoLayoutIsValidAndInsideDisplay() {
  const ui::DisplayLayout layout = ui::layoutFor(320, 170);
  TEST_ASSERT_EQUAL_UINT16(320, layout.width);
  TEST_ASSERT_EQUAL_UINT16(170, layout.height);
  assertAllInside(layout);
  TEST_ASSERT_TRUE(ui::validateLayout(layout));
}

void testIli9488LayoutIsValidAndInsideDisplay() {
  const ui::DisplayLayout layout = ui::layoutFor(480, 320);
  TEST_ASSERT_EQUAL_UINT16(480, layout.width);
  TEST_ASSERT_EQUAL_UINT16(320, layout.height);
  assertAllInside(layout);
  TEST_ASSERT_TRUE(ui::validateLayout(layout));
}

void testTopRowWidgetsDoNotOverlap() {
  for (const ui::DisplayLayout layout : {ui::layoutFor(320, 170),
                                         ui::layoutFor(480, 320)}) {
    TEST_ASSERT_FALSE(ui::overlaps(layout.trip1, layout.clock));
    TEST_ASSERT_FALSE(ui::overlaps(layout.clock, layout.trip2));
    TEST_ASSERT_FALSE(ui::overlaps(layout.trip1, layout.trip2));
  }
}

void testCompetitionWidgetsDoNotOverlap() {
  for (const ui::DisplayLayout layout : {ui::layoutFor(320, 170),
                                         ui::layoutFor(480, 320)}) {
    TEST_ASSERT_FALSE(ui::overlaps(layout.currentSegment, layout.delta));
    TEST_ASSERT_FALSE(ui::overlaps(layout.delta, layout.nextSegment));
    TEST_ASSERT_FALSE(
        ui::overlaps(layout.currentSegment, layout.nextSegment));
  }
}

void testDeltaFieldFitsRequiredValues() {
  for (const ui::DisplayLayout layout : {ui::layoutFor(320, 170),
                                         ui::layoutFor(480, 320)}) {
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("-1234", layout.deltaFont) <=
                     layout.delta.width);
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("0", layout.deltaFont) <=
                     layout.delta.width);
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("+1234", layout.deltaFont) <=
                     layout.delta.width);
  }
}

void testRepresentativeTextsFitTheirRegions() {
  for (const ui::DisplayLayout layout : {ui::layoutFor(320, 170),
                                         ui::layoutFor(480, 320)}) {
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("-12.34", layout.topFont) <=
                     layout.trip1.width);
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("23:59:59", layout.topFont) <=
                     layout.clock.width);
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("12-13", layout.segmentFont) <=
                     layout.currentSegment.width);
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("59:59", layout.segmentValueFont) <=
                     layout.currentSegment.width);
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("120", layout.segmentValueFont) <=
                     layout.nextSegment.width);
    TEST_ASSERT_TRUE(ui::estimatedTextWidth("120 km/h", layout.debugFont) <=
                     layout.debugSpeed.width);
  }
}

void testMissingNextSegmentCannotMoveDeltaGeometry() {
  const ui::DisplayLayout withNext = ui::layoutFor(480, 320);
  const ui::DisplayLayout withoutNext = ui::layoutFor(480, 320);
  TEST_ASSERT_EQUAL_INT16(withNext.delta.x, withoutNext.delta.x);
  TEST_ASSERT_EQUAL_INT16(withNext.delta.width, withoutNext.delta.width);
}

void testDebugSpeedDoesNotOverlapCompetitionValues() {
  for (const ui::DisplayLayout layout : {ui::layoutFor(320, 170),
                                         ui::layoutFor(480, 320)}) {
    TEST_ASSERT_FALSE(
        ui::overlaps(layout.debugSpeed, layout.currentSegment));
    TEST_ASSERT_FALSE(ui::overlaps(layout.debugSpeed, layout.delta));
    TEST_ASSERT_FALSE(ui::overlaps(layout.debugSpeed, layout.nextSegment));
  }
}

void testProfilesUseDifferentMeasurementsAndSameSemanticOrder() {
  const ui::DisplayLayout small = ui::layoutFor(320, 170);
  const ui::DisplayLayout large = ui::layoutFor(480, 320);
  TEST_ASSERT_NOT_EQUAL(small.delta.width, large.delta.width);
  TEST_ASSERT_TRUE(small.trip1.right() <= small.clock.x);
  TEST_ASSERT_TRUE(small.trip2.right() <= small.clock.x);
  TEST_ASSERT_TRUE(small.trip1.bottom() <= small.trip2.y);
  TEST_ASSERT_TRUE(large.trip1.right() <= large.clock.x);
  TEST_ASSERT_TRUE(large.trip2.right() <= large.clock.x);
  TEST_ASSERT_TRUE(large.trip1.bottom() <= large.trip2.y);
  TEST_ASSERT_TRUE(small.currentSegment.x < small.delta.x);
  TEST_ASSERT_TRUE(small.delta.x < small.nextSegment.x);
  TEST_ASSERT_TRUE(large.currentSegment.x < large.delta.x);
  TEST_ASSERT_TRUE(large.delta.x < large.nextSegment.x);
}

void testBottomTenPercentIsReservedForFooter() {
  for (const ui::DisplayLayout layout : {ui::layoutFor(320, 170),
                                         ui::layoutFor(480, 320)}) {
    const int16_t footerStart = static_cast<int16_t>(layout.height * 9 / 10);
    TEST_ASSERT_TRUE(layout.debugSpeed.y >= footerStart);
    TEST_ASSERT_EQUAL_INT16(layout.height, layout.debugSpeed.bottom());
    TEST_ASSERT_TRUE(layout.currentSegment.bottom() <= footerStart);
    TEST_ASSERT_TRUE(layout.delta.bottom() <= footerStart);
    TEST_ASSERT_TRUE(layout.nextSegment.bottom() <= footerStart);
  }
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testLilygoLayoutIsValidAndInsideDisplay);
  RUN_TEST(testIli9488LayoutIsValidAndInsideDisplay);
  RUN_TEST(testTopRowWidgetsDoNotOverlap);
  RUN_TEST(testCompetitionWidgetsDoNotOverlap);
  RUN_TEST(testDeltaFieldFitsRequiredValues);
  RUN_TEST(testRepresentativeTextsFitTheirRegions);
  RUN_TEST(testMissingNextSegmentCannotMoveDeltaGeometry);
  RUN_TEST(testDebugSpeedDoesNotOverlapCompetitionValues);
  RUN_TEST(testProfilesUseDifferentMeasurementsAndSameSemanticOrder);
  RUN_TEST(testBottomTenPercentIsReservedForFooter);
  return UNITY_END();
}
