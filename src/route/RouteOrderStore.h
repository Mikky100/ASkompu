#pragma once

#include "domain/RouteOrder.h"

namespace route {

class RouteOrderStore {
 public:
  virtual ~RouteOrderStore() {}
  virtual bool load(domain::RouteOrder& order) = 0;
  virtual bool replace(const domain::RouteOrder& validatedOrder) = 0;
  virtual bool clear() = 0;
};

}  // namespace route
