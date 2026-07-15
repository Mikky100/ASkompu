#include "RouteOrderCodec.h"

namespace route {
namespace {

constexpr uint32_t MAGIC = 0x524F5244UL;  // RORD
constexpr size_t HEADER_SIZE = 16;
constexpr size_t SEGMENT_SIZE = 21;

void put16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value));
  out.push_back(static_cast<uint8_t>(value >> 8));
}
void put32(std::vector<uint8_t>& out, uint32_t value) {
  for (uint8_t shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<uint8_t>(value >> shift));
  }
}
uint16_t get16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         static_cast<uint16_t>(bytes[1] << 8);
}
uint32_t get32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

}  // namespace

uint32_t RouteOrderCodec::checksum(const uint8_t* bytes, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t index = 0; index < length; ++index) {
    crc ^= bytes[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

bool RouteOrderCodec::encode(const domain::RouteOrder& order,
                             std::vector<uint8_t>& bytes) {
  if (domain::validateRouteOrder(order) !=
          domain::RouteOrderValidationError::NONE ||
      order.segments.size() > 0xFFFFU) {
    return false;
  }
  bytes.clear();
  bytes.reserve(HEADER_SIZE + order.segments.size() * SEGMENT_SIZE);
  put32(bytes, MAGIC);
  put16(bytes, order.schemaVersion);
  put16(bytes, static_cast<uint16_t>(order.segments.size()));
  bytes.push_back(static_cast<uint8_t>(order.competitionType));
  bytes.push_back(order.startHour);
  bytes.push_back(order.startMinute);
  bytes.push_back(0);
  put32(bytes, 0);
  for (const domain::SegmentDefinition& segment : order.segments) {
    put16(bytes, segment.segmentIndex);
    put16(bytes, segment.startPointIndex);
    put16(bytes, segment.endPointIndex);
    bytes.push_back(static_cast<uint8_t>(segment.segmentType));
    bytes.push_back(static_cast<uint8_t>(segment.pointTypeAtEnd));
    put32(bytes, segment.value);
    bytes.push_back(segment.hasMittisDuration ? 1 : 0);
    put16(bytes, segment.mittisDurationSeconds);
    bytes.push_back(segment.hasJatType ? 1 : 0);
    bytes.push_back(static_cast<uint8_t>(segment.jatType));
    bytes.push_back(segment.hasJatOffsetMinutes ? 1 : 0);
    put16(bytes, static_cast<uint16_t>(segment.jatOffsetMinutes));
    bytes.push_back(0);
  }
  const uint32_t crc = checksum(bytes.data() + HEADER_SIZE,
                                bytes.size() - HEADER_SIZE);
  bytes[12] = static_cast<uint8_t>(crc);
  bytes[13] = static_cast<uint8_t>(crc >> 8);
  bytes[14] = static_cast<uint8_t>(crc >> 16);
  bytes[15] = static_cast<uint8_t>(crc >> 24);
  return true;
}

bool RouteOrderCodec::decode(const uint8_t* bytes, size_t length,
                             domain::RouteOrder& order) {
  if (!bytes || length < HEADER_SIZE || get32(bytes) != MAGIC) {
    return false;
  }
  const uint16_t count = get16(bytes + 6);
  if (length != HEADER_SIZE + static_cast<size_t>(count) * SEGMENT_SIZE ||
      get32(bytes + 12) != checksum(bytes + HEADER_SIZE, length - HEADER_SIZE)) {
    return false;
  }
  domain::RouteOrder decoded;
  decoded.schemaVersion = get16(bytes + 4);
  decoded.competitionType = static_cast<domain::CompetitionType>(bytes[8]);
  decoded.startHour = bytes[9];
  decoded.startMinute = bytes[10];
  decoded.segments.reserve(count);
  size_t position = HEADER_SIZE;
  for (uint16_t index = 0; index < count; ++index, position += SEGMENT_SIZE) {
    domain::SegmentDefinition segment;
    segment.segmentIndex = get16(bytes + position);
    segment.startPointIndex = get16(bytes + position + 2);
    segment.endPointIndex = get16(bytes + position + 4);
    segment.segmentType = static_cast<domain::SegmentType>(bytes[position + 6]);
    segment.pointTypeAtEnd = static_cast<domain::PointType>(bytes[position + 7]);
    segment.value = get32(bytes + position + 8);
    segment.hasMittisDuration = bytes[position + 12] != 0;
    segment.mittisDurationSeconds = get16(bytes + position + 13);
    segment.hasJatType = bytes[position + 15] != 0;
    segment.jatType = static_cast<domain::JatType>(bytes[position + 16]);
    segment.hasJatOffsetMinutes = bytes[position + 17] != 0;
    segment.jatOffsetMinutes = static_cast<int16_t>(get16(bytes + position + 18));
    decoded.segments.push_back(segment);
  }
  if (domain::validateRouteOrder(decoded) !=
      domain::RouteOrderValidationError::NONE) {
    return false;
  }
  order = decoded;
  return true;
}

}  // namespace route
