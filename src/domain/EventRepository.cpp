#include "EventRepository.h"

#include <limits>

namespace domain {

bool RamEventRepository::append(EventRecord record,
                                uint64_t& assignedEventId) {
  if (nextEventId_ == 0) return false;
  record.eventId = nextEventId_;
  records_.push_back(record);
  assignedEventId = nextEventId_;
  if (nextEventId_ == std::numeric_limits<uint64_t>::max())
    nextEventId_ = 0;
  else
    ++nextEventId_;
  return true;
}

bool RamEventRepository::markCancelled(uint64_t eventId) {
  for (EventRecord& record : records_) {
    if (record.eventId == eventId) {
      record.cancelled = true;
      return true;
    }
  }
  return false;
}

const EventRecord* RamEventRepository::at(size_t index) const {
  return index < records_.size() ? &records_[index] : nullptr;
}

void RamEventRepository::clear() {
  records_.clear();
  nextEventId_ = 1;
}

std::vector<StageResult> stageResultsFromEvents(
    const EventRepository& repository) {
  std::vector<StageResult> results;
  for (size_t index = 0; index < repository.count(); ++index) {
    const EventRecord* record = repository.at(index);
    if (record && !record->cancelled && record->payload.hasStageResult)
      results.push_back(record->payload.stageResult);
  }
  return results;
}

}  // namespace domain
