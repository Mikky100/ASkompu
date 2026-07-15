#pragma once

#include "route/RouteOrderStore.h"

namespace settings {

class PreferencesRouteOrderStore : public route::RouteOrderStore {
 public:
  bool load(domain::RouteOrder& order) override;
  bool replace(const domain::RouteOrder& validatedOrder) override;
  bool clear() override;
};

}  // namespace settings
