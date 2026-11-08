#include "CompetitionProgressStore.h"

#include <Arduino.h>
#include <Preferences.h>

namespace settings {
namespace {

constexpr char NAMESPACE[] = "askompuRun";
constexpr char SNAPSHOT_KEY[] = "progress";
constexpr uint32_t MAGIC = 0x41534B50UL;

struct StoredProgress {
  uint32_t magic;
  domain::CompetitionProgressSnapshot snapshot;
  uint32_t checksum;
};

uint32_t checksum(const StoredProgress& stored) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&stored);
  const size_t length = sizeof(stored) - sizeof(stored.checksum);
  uint32_t hash = 2166136261UL;
  for (size_t index = 0; index < length; ++index) {
    hash ^= bytes[index];
    hash *= 16777619UL;
  }
  return hash;
}

}  // namespace

bool CompetitionProgressStore::load(
    domain::CompetitionProgressSnapshot& snapshot) {
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, true)) return false;
  StoredProgress stored{};
  const bool validLength =
      preferences.getBytesLength(SNAPSHOT_KEY) == sizeof(stored);
  const bool read = validLength &&
      preferences.getBytes(SNAPSHOT_KEY, &stored, sizeof(stored)) ==
          sizeof(stored);
  preferences.end();
  if (!read || stored.magic != MAGIC || stored.checksum != checksum(stored) ||
      stored.snapshot.schemaVersion != 2) {
    Serial.println("Competition progress load: not found or invalid");
    return false;
  }
  snapshot = stored.snapshot;
  Serial.printf("Competition progress load: state=%u stage=%u segment=%u\n",
                static_cast<unsigned>(snapshot.state),
                static_cast<unsigned>(snapshot.stageIndex),
                static_cast<unsigned>(snapshot.currentSegmentIndex));
  return true;
}

bool CompetitionProgressStore::save(
    const domain::CompetitionProgressSnapshot& snapshot) {
  StoredProgress stored{};
  stored.magic = MAGIC;
  stored.snapshot = snapshot;
  stored.checksum = checksum(stored);
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) return false;
  const bool saved =
      preferences.putBytes(SNAPSHOT_KEY, &stored, sizeof(stored)) ==
      sizeof(stored);
  preferences.end();
  Serial.printf("Competition progress save: %s state=%u stage=%u segment=%u\n",
                saved ? "ok" : "failed",
                static_cast<unsigned>(snapshot.state),
                static_cast<unsigned>(snapshot.stageIndex),
                static_cast<unsigned>(snapshot.currentSegmentIndex));
  return saved;
}

bool CompetitionProgressStore::clear() {
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) return false;
  const bool cleared = preferences.remove(SNAPSHOT_KEY);
  preferences.end();
  Serial.printf("Competition progress clear: %s\n",
                cleared ? "ok" : "not present");
  return cleared;
}

}  // namespace settings
