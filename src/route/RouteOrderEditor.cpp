#include "RouteOrderEditor.h"

#include <limits>

namespace route {
namespace {

uint32_t power10(uint8_t exponent) {
  uint32_t value = 1;
  while (exponent-- > 0) value *= 10;
  return value;
}

domain::JatType emitJatType(uint8_t selection) {
  static const domain::JatType TYPES[] = {domain::JatType::EMIT_JAT_OFFSET,
                                         domain::JatType::EMIT_MLA,
                                         domain::JatType::EMIT_ULA};
  return TYPES[selection % 3];
}

}  // namespace

void RouteOrderEditor::beginCreate() {
  draft_ = domain::RouteOrder{};
  draft_.competitionType = domain::CompetitionType::NON_EMIT;
  phase_ = EditorPhase::COMPETITION_TYPE;
  editingExisting_ = false;
  editingSegment_ = false;
  enteringMittisTime_ = false;
  saveFailed_ = false;
  selectedSegment_ = 0;
  startTimeField_ = 0;
}

void RouteOrderEditor::beginBrowse(const domain::RouteOrder& current) {
  draft_ = current;
  phase_ = EditorPhase::BROWSE;
  editingExisting_ = true;
  editingSegment_ = false;
  enteringMittisTime_ = false;
  saveFailed_ = false;
  selectedSegment_ = 0;
}

void RouteOrderEditor::beginNewSegment(domain::SegmentType type) {
  working_ = domain::SegmentDefinition{};
  working_.segmentIndex = static_cast<uint16_t>(draft_.segments.size());
  working_.startPointIndex = working_.segmentIndex;
  working_.endPointIndex = working_.segmentIndex + 1;
  working_.segmentType = type;
  phase_ = type == domain::SegmentType::MITTIS ? EditorPhase::VALUE
                                               : EditorPhase::SEGMENT_TYPE;
  digitCursor_ = 0;
  continuation_ = 0;
  editingSegment_ = false;
  enteringMittisTime_ = false;
}

void RouteOrderEditor::beginEditSelected() {
  if (selectedSegment_ == 0 || selectedSegment_ > draft_.segments.size()) return;
  working_ = draft_.segments[selectedSegment_ - 1];
  originalWorking_ = working_;
  phase_ = EditorPhase::SEGMENT_TYPE;
  digitCursor_ = 0;
  editingSegment_ = true;
  enteringMittisTime_ = false;
  saveFailed_ = false;
}

uint8_t RouteOrderEditor::digitCount() const {
  if (working_.segmentType == domain::SegmentType::TIME ||
      enteringMittisTime_)
    return 4;
  if (working_.segmentType == domain::SegmentType::SPEED) return 2;
  return 4;
}

bool RouteOrderEditor::canSelectMittis() const {
  for (size_t index = 0; index < draft_.segments.size(); ++index) {
    if (editingSegment_ && index + 1 == selectedSegment_) continue;
    if (draft_.segments[index].segmentType == domain::SegmentType::MITTIS)
      return false;
  }
  return true;
}

void RouteOrderEditor::adjustValue(int8_t direction) {
  if (working_.segmentType == domain::SegmentType::TIME ||
      enteringMittisTime_) {
    const uint32_t current = enteringMittisTime_
                                 ? working_.mittisDurationSeconds
                                 : working_.value;
    uint8_t digits[4] = {static_cast<uint8_t>((current / 600) % 10),
                         static_cast<uint8_t>((current / 60) % 10),
                         static_cast<uint8_t>((current / 10) % 6),
                         static_cast<uint8_t>(current % 10)};
    const uint8_t modulus = digitCursor_ == 2 ? 6 : 10;
    digits[digitCursor_] =
        static_cast<uint8_t>((digits[digitCursor_] + modulus + direction) % modulus);
    const uint16_t changed = static_cast<uint16_t>(
        (digits[0] * 10 + digits[1]) * 60 + digits[2] * 10 + digits[3]);
    if (enteringMittisTime_)
      working_.mittisDurationSeconds = changed;
    else
      working_.value = changed;
    return;
  }
  const uint8_t count = digitCount();
  const uint32_t place = power10(static_cast<uint8_t>(count - digitCursor_ - 1));
  const uint8_t digit = static_cast<uint8_t>((working_.value / place) % 10);
  const uint8_t next = static_cast<uint8_t>((digit + 10 + direction) % 10);
  const uint64_t candidate = static_cast<uint64_t>(working_.value) -
                             static_cast<uint64_t>(digit) * place +
                             static_cast<uint64_t>(next) * place;
  if (candidate <= std::numeric_limits<uint32_t>::max()) {
    working_.value = static_cast<uint32_t>(candidate);
  }
}

bool RouteOrderEditor::acceptValue() {
  if (working_.segmentType == domain::SegmentType::MITTIS &&
      !enteringMittisTime_) {
    if (working_.value < 1000 || working_.value > 9999) return false;
    working_.hasMittisDuration = true;
    working_.mittisDurationSeconds = 0;
    enteringMittisTime_ = true;
    digitCursor_ = 0;
    return true;
  }
  if (enteringMittisTime_ &&
      (working_.mittisDurationSeconds == 0 ||
       working_.mittisDurationSeconds > 3599)) {
    return false;
  }
  if (working_.value == 0 ||
      (working_.segmentType == domain::SegmentType::TIME &&
       working_.value > 3599) ||
      (working_.segmentType == domain::SegmentType::SPEED &&
       working_.value > 99) ||
      (working_.segmentType == domain::SegmentType::MITTIS &&
       (working_.value < 1000 || working_.value > 9999))) {
    return false;
  }
  if (editingSegment_) {
    const size_t editedIndex = selectedSegment_ - 1;
    const domain::SegmentDefinition old = draft_.segments[editedIndex];
    working_.pointTypeAtEnd = old.pointTypeAtEnd;
    working_.hasJatType = old.hasJatType;
    working_.jatType = old.jatType;
    working_.hasJatOffsetMinutes = old.hasJatOffsetMinutes;
    working_.jatOffsetMinutes = old.jatOffsetMinutes;
    draft_.segments[editedIndex] = working_;
    if (domain::validateRouteOrder(draft_) !=
        domain::RouteOrderValidationError::NONE) {
      draft_.segments[editedIndex] = old;
      return false;
    }
    phase_ = EditorPhase::SAVE_PENDING;
    return true;
  }
  phase_ = EditorPhase::CONTINUATION;
  return true;
}

EditorResult RouteOrderEditor::finishWorkingSegment(
    domain::PointType pointType) {
  working_.pointTypeAtEnd = pointType;
  if (pointType != domain::PointType::JAT) {
    working_.hasJatType = false;
    working_.hasJatOffsetMinutes = false;
  }
  draft_.segments.push_back(working_);
  if (pointType == domain::PointType::FINISH_M) {
    if (domain::validateRouteOrder(draft_) !=
        domain::RouteOrderValidationError::NONE) {
      draft_.segments.pop_back();
      return EditorResult::NONE;
    }
    phase_ = EditorPhase::SAVE_PENDING;
    return EditorResult::SAVE_REQUESTED;
  }
  beginNewSegment();
  return EditorResult::NONE;
}

EditorResult RouteOrderEditor::acceptContinuation() {
  if (continuation_ == 0) {
    return finishWorkingSegment(domain::PointType::NORMAL);
  }
  if (continuation_ == 1) {
    if (draft_.competitionType == domain::CompetitionType::NON_EMIT) {
      working_.hasJatType = true;
      working_.jatType = domain::JatType::MANNED_JAT;
      working_.hasJatOffsetMinutes = true;
      phase_ = EditorPhase::JAT_OFFSET;
      return EditorResult::NONE;
    }
    jatSelection_ = 0;
    phase_ = EditorPhase::JAT_TYPE;
    return EditorResult::NONE;
  }
  return finishWorkingSegment(domain::PointType::FINISH_M);
}

EditorResult RouteOrderEditor::handle(EditorKey key, bool longPress) {
  saveFailed_ = false;
  if (longPress && key == EditorKey::LEFT &&
      phase_ != EditorPhase::SAVE_PENDING) {
    if (!editingExisting_) {
      phaseBeforeCancel_ = phase_;
      phase_ = EditorPhase::CANCEL_PROMPT;
      return EditorResult::NONE;
    }
    return EditorResult::EXIT;
  }
  if (phase_ == EditorPhase::CANCEL_PROMPT) {
    if (key == EditorKey::LEFT) phase_ = phaseBeforeCancel_;
    if (key == EditorKey::RIGHT) return EditorResult::EXIT;
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::SAVE_PENDING) return EditorResult::NONE;

  if (phase_ == EditorPhase::COMPETITION_TYPE) {
    if (key == EditorKey::UP || key == EditorKey::DOWN) {
      draft_.competitionType =
          draft_.competitionType == domain::CompetitionType::EMIT
              ? domain::CompetitionType::NON_EMIT
              : domain::CompetitionType::EMIT;
    } else if (key == EditorKey::RIGHT) {
      phase_ = EditorPhase::START_TIME;
      startTimeField_ = 0;
    } else if (key == EditorKey::LEFT) {
      return EditorResult::EXIT;
    }
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::START_TIME) {
    if (key == EditorKey::UP || key == EditorKey::DOWN) {
      const int8_t direction = key == EditorKey::UP ? 1 : -1;
      uint8_t& value = startTimeField_ == 0 ? draft_.startHour
                                            : draft_.startMinute;
      const uint8_t modulus = startTimeField_ == 0 ? 24 : 60;
      value = static_cast<uint8_t>((value + modulus + direction) % modulus);
    } else if (key == EditorKey::RIGHT) {
      if (startTimeField_ == 0)
        startTimeField_ = 1;
      else
        beginNewSegment();
    } else if (key == EditorKey::LEFT && startTimeField_ == 1) {
      startTimeField_ = 0;
    }
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::BROWSE) {
    if (key == EditorKey::UP && selectedSegment_ > 0) --selectedSegment_;
    if (key == EditorKey::DOWN && selectedSegment_ < draft_.segments.size())
      ++selectedSegment_;
    if (key == EditorKey::RIGHT) beginEditSelected();
    if (key == EditorKey::LEFT) return EditorResult::EXIT;
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::SEGMENT_TYPE) {
    if (key == EditorKey::UP || key == EditorKey::DOWN) {
      const uint8_t current = static_cast<uint8_t>(working_.segmentType);
      const uint8_t typeCount = canSelectMittis() ? 3 : 2;
      const uint8_t next = static_cast<uint8_t>(
          (current + (key == EditorKey::UP ? typeCount - 1 : 1)) %
          typeCount);
      working_.segmentType = static_cast<domain::SegmentType>(next);
      working_.value = 0;
      working_.hasMittisDuration = false;
      working_.mittisDurationSeconds = 0;
      enteringMittisTime_ = false;
    } else if (key == EditorKey::RIGHT) {
      phase_ = EditorPhase::VALUE;
      digitCursor_ = 0;
    } else if (key == EditorKey::LEFT) {
      if (editingSegment_) {
        phase_ = EditorPhase::BROWSE;
      }
    }
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::VALUE) {
    if (key == EditorKey::UP) adjustValue(1);
    if (key == EditorKey::DOWN) adjustValue(-1);
    if (key == EditorKey::LEFT) {
      if (digitCursor_ > 0) --digitCursor_;
      else if (enteringMittisTime_) {
        enteringMittisTime_ = false;
        digitCursor_ = 3;
      } else
        phase_ = EditorPhase::SEGMENT_TYPE;
    }
    if (key == EditorKey::RIGHT) {
      if (digitCursor_ + 1 < digitCount()) {
        ++digitCursor_;
      } else if (acceptValue() && editingSegment_) {
        return EditorResult::SAVE_REQUESTED;
      }
    }
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::CONTINUATION) {
    if (key == EditorKey::UP)
      continuation_ = static_cast<uint8_t>((continuation_ + 2) % 3);
    if (key == EditorKey::DOWN)
      continuation_ = static_cast<uint8_t>((continuation_ + 1) % 3);
    if (key == EditorKey::LEFT) {
      phase_ = EditorPhase::VALUE;
      digitCursor_ = digitCount() - 1;
    }
    if (key == EditorKey::RIGHT) return acceptContinuation();
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::JAT_TYPE) {
    if (draft_.competitionType == domain::CompetitionType::EMIT &&
        (key == EditorKey::UP || key == EditorKey::DOWN)) {
      jatSelection_ = static_cast<uint8_t>(
          (jatSelection_ + (key == EditorKey::UP ? 2 : 1)) % 3);
    }
    if (key == EditorKey::LEFT) phase_ = EditorPhase::CONTINUATION;
    if (key == EditorKey::RIGHT) {
      working_.hasJatType = true;
      working_.jatType = draft_.competitionType == domain::CompetitionType::EMIT
                             ? emitJatType(jatSelection_)
                             : domain::JatType::MANNED_JAT;
      const bool offset = working_.jatType == domain::JatType::MANNED_JAT ||
                          working_.jatType == domain::JatType::EMIT_JAT_OFFSET;
      working_.hasJatOffsetMinutes = offset;
      if (offset) phase_ = EditorPhase::JAT_OFFSET;
      else return finishWorkingSegment(domain::PointType::JAT);
    }
    return EditorResult::NONE;
  }
  if (phase_ == EditorPhase::JAT_OFFSET) {
    if (key == EditorKey::UP &&
        working_.jatOffsetMinutes < std::numeric_limits<int16_t>::max())
      ++working_.jatOffsetMinutes;
    if (key == EditorKey::DOWN &&
        working_.jatOffsetMinutes > std::numeric_limits<int16_t>::min())
      --working_.jatOffsetMinutes;
    if (key == EditorKey::LEFT)
      phase_ = draft_.competitionType == domain::CompetitionType::NON_EMIT
                   ? EditorPhase::CONTINUATION
                   : EditorPhase::JAT_TYPE;
    if (key == EditorKey::RIGHT)
      return finishWorkingSegment(domain::PointType::JAT);
  }
  return EditorResult::NONE;
}

void RouteOrderEditor::completeSave(bool succeeded) {
  if (phase_ != EditorPhase::SAVE_PENDING) return;
  if (succeeded) {
    editingExisting_ = true;
    editingSegment_ = false;
    phase_ = EditorPhase::BROWSE;
    selectedSegment_ = 0;
  } else {
    saveFailed_ = true;
    if (editingSegment_) {
      draft_.segments[selectedSegment_ - 1] = originalWorking_;
      phase_ = EditorPhase::VALUE;
      digitCursor_ = digitCount() - 1;
    } else {
      draft_.segments.pop_back();
      phase_ = EditorPhase::CONTINUATION;
      continuation_ = 2;
    }
  }
}

RouteOrderEditorView RouteOrderEditor::view() const {
  return {phase_,
          editingExisting_,
          saveFailed_,
          draft_.competitionType,
          draft_.startHour,
          draft_.startMinute,
          startTimeField_,
          working_.segmentType,
          enteringMittisTime_ ? working_.mittisDurationSeconds : working_.value,
          enteringMittisTime_,
          digitCursor_,
          continuation_,
          draft_.competitionType == domain::CompetitionType::EMIT
              ? emitJatType(jatSelection_)
              : domain::JatType::MANNED_JAT,
          working_.jatOffsetMinutes,
          selectedSegment_,
          static_cast<uint16_t>(draft_.segments.size())};
}

}  // namespace route
