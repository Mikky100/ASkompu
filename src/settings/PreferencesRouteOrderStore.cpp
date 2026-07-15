#include "PreferencesRouteOrderStore.h"

#include <Preferences.h>
#include <vector>

#include "route/RouteOrderCodec.h"

namespace settings {
namespace {

constexpr char NAMESPACE[] = "askompuOrder";
constexpr char SLOT_KEYS[][4] = {"roA", "roB"};
constexpr char GENERATION_KEYS[][5] = {"genA", "genB"};

bool readSlot(Preferences& preferences, uint8_t slot, domain::RouteOrder& order,
              uint32_t& generation) {
  generation = preferences.getUInt(GENERATION_KEYS[slot], 0);
  const size_t length = preferences.getBytesLength(SLOT_KEYS[slot]);
  if (generation == 0 || length == 0) {
    return false;
  }
  std::vector<uint8_t> bytes(length);
  if (preferences.getBytes(SLOT_KEYS[slot], bytes.data(), length) != length) {
    return false;
  }
  return route::RouteOrderCodec::decode(bytes.data(), bytes.size(), order);
}

bool verifySlotPayload(Preferences& preferences, uint8_t slot,
                       size_t expectedLength) {
  const size_t length = preferences.getBytesLength(SLOT_KEYS[slot]);
  if (length != expectedLength) return false;
  std::vector<uint8_t> bytes(length);
  domain::RouteOrder decoded;
  return preferences.getBytes(SLOT_KEYS[slot], bytes.data(), length) == length &&
         route::RouteOrderCodec::decode(bytes.data(), bytes.size(), decoded);
}

}  // namespace

bool PreferencesRouteOrderStore::load(domain::RouteOrder& order) {
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, true)) {
    return false;
  }
  domain::RouteOrder candidates[2];
  uint32_t generations[2]{};
  const bool validA = readSlot(preferences, 0, candidates[0], generations[0]);
  const bool validB = readSlot(preferences, 1, candidates[1], generations[1]);
  preferences.end();
  if (!validA && !validB) {
    return false;
  }
  const uint8_t selected = validB && (!validA || generations[1] > generations[0])
                               ? 1
                               : 0;
  order = candidates[selected];
  return true;
}

bool PreferencesRouteOrderStore::replace(
    const domain::RouteOrder& validatedOrder) {
  std::vector<uint8_t> bytes;
  if (!route::RouteOrderCodec::encode(validatedOrder, bytes)) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) {
    return false;
  }
  domain::RouteOrder ignored;
  uint32_t generations[2]{};
  const bool validA = readSlot(preferences, 0, ignored, generations[0]);
  const bool validB = readSlot(preferences, 1, ignored, generations[1]);
  const uint8_t active = validB && (!validA || generations[1] > generations[0])
                             ? 1
                             : 0;
  const uint8_t target = (validA || validB) ? static_cast<uint8_t>(1 - active) : 0;
  const uint32_t nextGeneration =
      (generations[0] > generations[1] ? generations[0] : generations[1]) + 1;
  const bool dataWritten =
      preferences.putBytes(SLOT_KEYS[target], bytes.data(), bytes.size()) ==
      bytes.size();
  const bool generationWritten =
      dataWritten && verifySlotPayload(preferences, target, bytes.size()) &&
      preferences.putUInt(GENERATION_KEYS[target], nextGeneration) ==
          sizeof(uint32_t);
  preferences.end();
  return generationWritten;
}

bool PreferencesRouteOrderStore::clear() {
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) {
    return false;
  }
  const bool cleared = preferences.clear();
  preferences.end();
  return cleared;
}

}  // namespace settings
