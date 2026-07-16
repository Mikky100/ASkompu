#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "CompetitionRecords.h"

namespace domain {

class EventRepository {
 public:
  virtual ~EventRepository() {}
  virtual bool append(EventRecord record, uint64_t& assignedEventId) = 0;
  virtual bool markCancelled(uint64_t eventId) = 0;
  virtual size_t count() const = 0;
  virtual const EventRecord* at(size_t index) const = 0;
};

class RamEventRepository : public EventRepository {
 public:
  bool append(EventRecord record, uint64_t& assignedEventId) override;
  bool markCancelled(uint64_t eventId) override;
  size_t count() const override { return records_.size(); }
  const EventRecord* at(size_t index) const override;
  void clear();

 private:
  std::vector<EventRecord> records_;
  uint64_t nextEventId_ = 1;
};

std::vector<StageResult> stageResultsFromEvents(
    const EventRepository& repository);

}  // namespace domain
