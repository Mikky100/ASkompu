#pragma once

#include "domain/CompetitionEngine.h"

namespace settings {

class CompetitionProgressStore {
 public:
  bool load(domain::CompetitionProgressSnapshot& snapshot);
  bool save(const domain::CompetitionProgressSnapshot& snapshot);
  bool clear();
};

}  // namespace settings
