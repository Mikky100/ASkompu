#include "DisplayView.h"

#include <inttypes.h>
#include <cstdio>

#include "BoardConfig.h"

namespace ui {
namespace {

void formatTrip(char* text, size_t size, uint64_t distanceMm) {
  const uint64_t meters = distanceMm / 1000ULL;
  if (meters < 10000ULL) {
    std::snprintf(text, size, "%" PRIu64 ".%03" PRIu64,
                  meters / 1000ULL, meters % 1000ULL);
  } else {
    const uint64_t tenMeters = meters / 10ULL;
    std::snprintf(text, size, "%" PRIu64 ".%02" PRIu64,
                  tenMeters / 100ULL, tenMeters % 100ULL);
  }
}

void formatEditableValue(char* text, size_t size,
                         const route::RouteOrderEditorView& editor) {
  char digits[12]{};
  uint8_t count = 0;
  if (editor.segmentType == domain::SegmentType::TIME ||
      editor.enteringMittisTime) {
    std::snprintf(digits, sizeof(digits), "%02lu%02lu",
                  static_cast<unsigned long>(editor.value / 60),
                  static_cast<unsigned long>(editor.value % 60));
    count = 4;
  } else if (editor.segmentType == domain::SegmentType::SPEED) {
    std::snprintf(digits, sizeof(digits), "%02lu",
                  static_cast<unsigned long>(editor.value));
    count = 2;
  } else {
    std::snprintf(digits, sizeof(digits), "%04lu",
                  static_cast<unsigned long>(editor.value));
    count = 4;
  }
  size_t output = 0;
  for (uint8_t index = 0; index < count && output + 4 < size; ++index) {
    if (index == editor.digitCursor) text[output++] = '[';
    text[output++] = digits[index];
    if (index == editor.digitCursor) text[output++] = ']';
    if (editor.segmentType == domain::SegmentType::TIME && index == 1)
      text[output++] = ':';
  }
  text[output] = '\0';
}

}  // namespace

DisplayView::DisplayView() : canvas_(&display_) {}

void DisplayView::begin() {
  display_.init();
  display_.setRotation(1);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
  canvas_.setColorDepth(16);
  canvas_.createSprite(BoardConfig::DISPLAY_WIDTH, BoardConfig::DISPLAY_HEIGHT);
  canvas_.fillSprite(TFT_BLACK);
  canvas_.pushSprite(0, 0);
}

void DisplayView::render(const core::DisplayModel& model) {
  switch (model.textColor) {
    case domain::TextColor::RED:
      textColor_ = TFT_RED;
      break;
    case domain::TextColor::GREEN:
      textColor_ = TFT_GREEN;
      break;
    case domain::TextColor::WHITE:
    default:
      textColor_ = TFT_WHITE;
      break;
  }
  canvas_.fillSprite(TFT_BLACK);
  canvas_.setTextSize(1);
  switch (model.screen) {
    case core::Screen::StartupTimeEntry:
    case core::Screen::TimeEdit:
      showTimeEntry(model.timeEntry);
      break;
    case core::Screen::BasicView:
      showBasicView(model);
      break;
    case core::Screen::Menu:
      showMenu(model.menu);
      break;
    case core::Screen::CalibrationEdit:
      showCalibration(model.calibration);
      break;
    case core::Screen::OrderEdit:
      showOrder(model.order);
      break;
    case core::Screen::Diagnostics:
      showDiagnostics(model.diagnostics);
      break;
  }
  canvas_.pushSprite(0, 0);
}

void DisplayView::showBasicView(const core::DisplayModel& model) {
  char clockText[12];
  char speedText[18];
  char trip1Text[24];
  char trip2Text[24];
  std::snprintf(clockText, sizeof(clockText), "%02u:%02u:%02u",
                model.clock.hour, model.clock.minute, model.clock.second);
  std::snprintf(speedText, sizeof(speedText), "%.0f km/h", model.speedKmh);
  formatTrip(trip1Text, sizeof(trip1Text), model.trip1.distanceMillimeters);
  formatTrip(trip2Text, sizeof(trip2Text), model.trip2.distanceMillimeters);

  canvas_.setTextDatum(TL_DATUM);
  canvas_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas_.drawString(clockText, 8, 7, 2);
  canvas_.setTextDatum(TR_DATUM);
  canvas_.drawString(speedText, BoardConfig::DISPLAY_WIDTH - 8, 7, 2);

  canvas_.setTextDatum(TL_DATUM);
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.drawString("TRIP 1", 8, 38, 2);
  canvas_.setTextDatum(ML_DATUM);
  canvas_.setTextColor(textColor_, TFT_BLACK);
  canvas_.setTextSize(2);
  canvas_.drawString(trip1Text, 8, 83, 2);
  canvas_.setTextSize(1);

  canvas_.drawFastHLine(8, 120, BoardConfig::DISPLAY_WIDTH - 16, TFT_DARKGREY);
  canvas_.setTextDatum(BL_DATUM);
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.drawString("TRIP 2", 8, 162, 2);
  canvas_.setTextDatum(BR_DATUM);
  canvas_.setTextColor(textColor_, TFT_BLACK);
  canvas_.drawString(trip2Text, BoardConfig::DISPLAY_WIDTH - 8, 162, 2);
}

void DisplayView::showTimeEntry(const core::TimeEntryDisplayModel& model) {
  char hour[4];
  char minute[4];
  std::snprintf(hour, sizeof(hour), "%u", model.hour);
  std::snprintf(minute, sizeof(minute), "%02u", model.minute);
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.drawString(model.startup ? "ASETA KELLONAIKA" : "MUUTA KELLONAIKA",
                     BoardConfig::DISPLAY_WIDTH / 2, 12, 2);
  canvas_.setTextSize(3);
  canvas_.setTextDatum(MR_DATUM);
  canvas_.setTextColor(model.activeField == core::TimeField::Hour ? TFT_YELLOW
                                                                  : textColor_,
                       TFT_BLACK);
  canvas_.drawString(hour, 145, 78, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextColor(textColor_, TFT_BLACK);
  canvas_.drawString(":", 160, 78, 2);
  canvas_.setTextDatum(ML_DATUM);
  canvas_.setTextColor(model.activeField == core::TimeField::Minute
                           ? TFT_YELLOW
                           : textColor_,
                       TFT_BLACK);
  canvas_.drawString(minute, 175, 78, 2);
  canvas_.setTextSize(1);
  canvas_.setTextDatum(BC_DATUM);
  canvas_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas_.drawString("YLOS/ALAS MUUTTAA   OIKEA JATKAA", 160, 160, 1);
}

void DisplayView::showMenu(const core::MenuDisplayModel& model) {
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.drawString(model.title, BoardConfig::DISPLAY_WIDTH / 2, 5, 2);
  constexpr int16_t top = 27;
  constexpr int16_t rowHeight = 27;
  for (uint8_t row = 0; row < model.visibleRowCount; ++row) {
    const bool selected = row == model.selectedVisibleRow;
    const uint16_t background = selected ? TFT_DARKGREY : TFT_BLACK;
    canvas_.fillRect(5, top + row * rowHeight,
                     BoardConfig::DISPLAY_WIDTH - 10, rowHeight - 2,
                     background);
    canvas_.setTextDatum(ML_DATUM);
    canvas_.setTextColor(model.rows[row].enabled ? textColor_ : TFT_DARKGREY,
                         background);
    canvas_.drawString(model.rows[row].label, 13,
                       top + row * rowHeight + rowHeight / 2, 2);
    if (!model.rows[row].enabled) {
      canvas_.setTextDatum(MR_DATUM);
      canvas_.drawString("--", BoardConfig::DISPLAY_WIDTH - 13,
                         top + row * rowHeight + rowHeight / 2, 2);
    }
  }
  if (model.scrollOffset > 0) {
    canvas_.fillTriangle(310, 29, 305, 36, 315, 36, TFT_CYAN);
  }
  if (model.scrollOffset + model.visibleRowCount < model.totalRows) {
    canvas_.fillTriangle(310, 163, 305, 156, 315, 156, TFT_CYAN);
  }
}

void DisplayView::showCalibration(
    const core::CalibrationDisplayModel& model) {
  char valueText[32];
  std::snprintf(valueText, sizeof(valueText), "%lu mm/pulssi",
                static_cast<unsigned long>(model.editedMillimetersPerPulse));
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.drawString("MITTARIKERROIN", BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, TFT_BLACK);
  canvas_.drawString(valueText, BoardConfig::DISPLAY_WIDTH / 2, 75, 2);
  canvas_.setTextSize(1);
  canvas_.setTextColor(model.saveFailed ? TFT_RED : TFT_LIGHTGREY, TFT_BLACK);
  canvas_.drawString(model.saveFailed ? "TALLENNUSVIRHE" : "YLOS/ALAS +/- 1",
                     BoardConfig::DISPLAY_WIDTH / 2, 118, 1);
  canvas_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas_.drawString("VASEN HYLKAA   OIKEA HYVAKSYY",
                     BoardConfig::DISPLAY_WIDTH / 2, 148, 1);
}

void DisplayView::showOrder(const core::OrderDisplayModel& model) {
  const route::RouteOrderEditorView& editor = model.editor;
  const char* title = "AJOMAARAYS";
  const char* primary = "";
  char value[48]{};
  switch (editor.phase) {
    case route::EditorPhase::COMPETITION_TYPE:
      title = "KILPAILUTYYPPI";
      primary = editor.competitionType == domain::CompetitionType::EMIT
                    ? "EMIT"
                    : "NON-EMIT";
      break;
    case route::EditorPhase::START_TIME:
      title = "KILPAILUN LAHTOAIKA";
      if (editor.startTimeField == 0)
        std::snprintf(value, sizeof(value), "[%02u]:%02u", editor.startHour,
                      editor.startMinute);
      else
        std::snprintf(value, sizeof(value), "%02u:[%02u]", editor.startHour,
                      editor.startMinute);
      primary = value;
      break;
    case route::EditorPhase::SEGMENT_TYPE:
      title = editor.editingExisting ? "MUOKKAA PISTEVALIA" : "MAARAYSTYYPPI";
      primary = editor.segmentType == domain::SegmentType::TIME
                    ? "AIKA"
                    : (editor.segmentType == domain::SegmentType::SPEED
                           ? "NOPEUS"
                           : "MITTIS");
      break;
    case route::EditorPhase::VALUE:
      title = editor.segmentType == domain::SegmentType::TIME
                  ? "AIKA"
                  : (editor.segmentType == domain::SegmentType::SPEED
                         ? "NOPEUS"
                         : (editor.enteringMittisTime ? "MITTIS AIKA"
                                                      : "MITTIS METRIA"));
      formatEditableValue(value, sizeof(value), editor);
      primary = value;
      break;
    case route::EditorPhase::CONTINUATION: {
      static const char* ACTIONS[] = {"SEURAAVA", "JAT", "MAALI"};
      title = "JATKOTOIMINTO";
      primary = ACTIONS[editor.continuation];
      break;
    }
    case route::EditorPhase::JAT_TYPE:
      title = "JAT-TYYPPI";
      switch (editor.jatType) {
        case domain::JatType::MANNED_JAT: primary = "MANNED JAT"; break;
        case domain::JatType::EMIT_JAT_OFFSET: primary = "JAT+AIKA"; break;
        case domain::JatType::EMIT_MLA: primary = "JAT+MLA"; break;
        case domain::JatType::EMIT_ULA: primary = "JAT+ULA"; break;
      }
      break;
    case route::EditorPhase::JAT_OFFSET:
      title = "JAT LISAAIKA";
      std::snprintf(value, sizeof(value), "%+d min", editor.jatOffsetMinutes);
      primary = value;
      break;
    case route::EditorPhase::BROWSE:
      title = "AJOMAARAYS";
      if (model.showsStartTime) {
        std::snprintf(value, sizeof(value), "LAHTO %02u:%02u",
                      editor.startHour, editor.startMinute);
        primary = value;
      } else if (model.hasSelectedSegment) {
        const domain::SegmentDefinition& segment = model.selectedSegment;
        char startPoint[8];
        char endPoint[8];
        if (segment.startPointIndex == 0)
          std::snprintf(startPoint, sizeof(startPoint), "L");
        else if (segment.segmentType == domain::SegmentType::MITTIS)
          std::snprintf(value, sizeof(value), "%s-%s MITTIS %lum %lu:%02lu",
                        startPoint, endPoint,
                        static_cast<unsigned long>(segment.value),
                        static_cast<unsigned long>(
                            segment.mittisDurationSeconds / 60),
                        static_cast<unsigned long>(
                            segment.mittisDurationSeconds % 60));
        else
          std::snprintf(startPoint, sizeof(startPoint), "%u",
                        segment.startPointIndex);
        if (segment.pointTypeAtEnd == domain::PointType::FINISH_M)
          std::snprintf(endPoint, sizeof(endPoint), "M");
        else
          std::snprintf(endPoint, sizeof(endPoint), "%u",
                        segment.endPointIndex);
        const char* type = segment.segmentType == domain::SegmentType::TIME
                               ? "AIKA"
                               : (segment.segmentType == domain::SegmentType::SPEED
                                      ? "NOPEUS"
                                      : "MITTIS");
        if (segment.segmentType == domain::SegmentType::TIME)
          std::snprintf(value, sizeof(value), "%s-%s %s %lu:%02lu",
                        startPoint, endPoint, type,
                        static_cast<unsigned long>(segment.value / 60),
                        static_cast<unsigned long>(segment.value % 60));
        else
          std::snprintf(value, sizeof(value), "%s-%s %s %lu",
                        startPoint, endPoint, type,
                        static_cast<unsigned long>(segment.value));
        primary = value;
      }
      break;
    case route::EditorPhase::CANCEL_PROMPT:
      title = "KESKEYTETAANKO?";
      primary = "VASEN EI   OIKEA KYLLA";
      break;
    case route::EditorPhase::REPLACE_PROMPT:
      title = "UUSI AJOMAARAYS?";
      primary = "VASEN EI   OIKEA KYLLA";
      break;
    case route::EditorPhase::SAVE_PENDING:
      title = "TALLENNETAAN";
      primary = "ODOTA";
      break;
  }
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.drawString(title, BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(editor.saveFailed ? TFT_RED : textColor_, TFT_BLACK);
  canvas_.drawString(primary, BoardConfig::DISPLAY_WIDTH / 2, 78, 2);
  canvas_.setTextSize(1);
  canvas_.setTextColor(editor.saveFailed ? TFT_RED : TFT_LIGHTGREY, TFT_BLACK);
  canvas_.drawString(editor.saveFailed ? "TALLENNUSVIRHE"
                                       : "YLOS/ALAS   VASEN   OIKEA",
                     BoardConfig::DISPLAY_WIDTH / 2, 150, 1);
}

void DisplayView::showDiagnostics(
    const core::DiagnosticsDisplayModel& model) {
  char line[48];
  canvas_.setTextDatum(TL_DATUM);
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.drawString("DIAGNOSTIIKKA", 4, 2, 2);
  canvas_.setTextColor(textColor_, TFT_BLACK);
  std::snprintf(line, sizeof(line), "L:%u U:%u D:%u R:%u GPIO14:%u",
                model.buttonPressed[0], model.buttonPressed[1],
                model.buttonPressed[2], model.buttonPressed[3],
                model.buttonPressed[4]);
  canvas_.drawString(line, 4, 20, 1);
  std::snprintf(line, sizeof(line), "LAST %u/%u  STATE %u",
                model.lastButtonId, model.lastButtonEventType,
                static_cast<unsigned>(model.currentScreen));
  canvas_.drawString(line, 4, 34, 1);
  std::snprintf(line, sizeof(line), "PULSSIT %" PRIu64 "  T1 %" PRIu64,
                model.totalPulseCount, model.trip1PulseCount);
  canvas_.drawString(line, 4, 48, 1);
  std::snprintf(line, sizeof(line), "T2 PULSSIT %" PRIu64,
                model.trip2PulseCount);
  canvas_.drawString(line, 4, 62, 1);
  std::snprintf(line, sizeof(line), "T1 %" PRIu64 " mm", model.trip1DistanceMillimeters);
  canvas_.drawString(line, 4, 76, 1);
  std::snprintf(line, sizeof(line), "T2 %" PRIu64 " mm", model.trip2DistanceMillimeters);
  canvas_.drawString(line, 4, 90, 1);
  std::snprintf(line, sizeof(line), "K %lu  V %.1f km/h",
                static_cast<unsigned long>(model.millimetersPerPulse),
                model.speedKmh);
  canvas_.drawString(line, 4, 104, 1);
  std::snprintf(line, sizeof(line), "PULSSI-IKA %lu ms  ZERO %u",
                static_cast<unsigned long>(model.lastPulseAgeMilliseconds),
                model.speedZeroTimedOut);
  canvas_.drawString(line, 4, 118, 1);
  std::snprintf(line, sizeof(line), "KELLO %02u:%02u:%02u  SET %u",
                model.clock.hour, model.clock.minute, model.clock.second,
                model.clockSet);
  canvas_.drawString(line, 4, 132, 1);
  std::snprintf(line, sizeof(line), "KULUNUT %" PRIu64 " ms",
                model.clockElapsedMilliseconds);
  canvas_.drawString(line, 4, 146, 1);
  canvas_.setTextDatum(BR_DATUM);
  canvas_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas_.drawString("VASEN PALAA", 316, 168, 1);
}

}  // namespace ui
