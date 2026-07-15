#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "domain/RouteOrder.h"

namespace route {

class RouteOrderCodec {
 public:
  static bool encode(const domain::RouteOrder& order,
                     std::vector<uint8_t>& bytes);
  static bool decode(const uint8_t* bytes, size_t length,
                     domain::RouteOrder& order);
  static uint32_t checksum(const uint8_t* bytes, size_t length);
};

}  // namespace route
