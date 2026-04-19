#pragma once

#include <vector>

#include "1tracker_pi/endpoint_config.h"

namespace tracker_pi {

struct RuntimeConfig {
  bool enabled = true;
  std::vector<EndpointConfig> endpoints;
};

}  // namespace tracker_pi
