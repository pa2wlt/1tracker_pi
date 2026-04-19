#include "1tracker_pi/payload_builder.h"
#include "1tracker_pi/endpoint_type_behavior.h"

namespace tracker_pi {

std::optional<std::string> PayloadBuilder::buildPayload(
    const Snapshot& snapshot, const EndpointConfig& endpoint) const {
  return getEndpointTypeBehavior(endpoint).buildPayload(snapshot, endpoint);
}

}  // namespace tracker_pi
