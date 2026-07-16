#pragma once

#include "domain/RouteOrder.h"

namespace route {

enum class RouteOrderActivity : uint8_t { INACTIVE = 0, ACTIVE = 1, COMPLETED = 2 };

class RouteOrderStore {
 public:
  virtual ~RouteOrderStore() {}
  virtual bool load(domain::RouteOrder& order) = 0;
  virtual bool replace(const domain::RouteOrder& validatedOrder) = 0;
  virtual bool clear() = 0;
  virtual RouteOrderActivity activity() { return RouteOrderActivity::INACTIVE; }
  virtual bool markCompleted() { return false; }
};

}  // namespace route
