#pragma once

#include <optional>
#include <string>

#include "1tracker_pi/endpoint_config.h"
#include "1tracker_pi/snapshot.h"

namespace tracker_pi {

class PayloadBuilder {
public:
  std::optional<std::string> buildPayload(const Snapshot& snapshot,
                                          const EndpointConfig& endpoint) const;
};

}  // namespace tracker_pi
