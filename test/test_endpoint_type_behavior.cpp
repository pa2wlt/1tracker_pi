#include <cstdlib>
#include <stdexcept>

#include "1tracker_pi/endpoint_type_behavior.h"
#include "1tracker_pi/nfl_settings.h"
#include "1tracker_pi/endpoint_policy.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  const auto types = tracker_pi::listEndpointTypes();
  expect(types.size() >= 2, "expected at least two endpoint types");
  expect(types.front() == tracker_pi::kEndpointTypeHttpJsonWithHeaderKey,
         "generic JSON type should be registered");

  tracker_pi::EndpointConfig generic;
  generic.type = tracker_pi::kEndpointTypeHttpJsonWithHeaderKey;
  generic.sendIntervalMinutes = 0;
  tracker_pi::getEndpointTypeBehavior(generic).applyDefaults(generic);
  expect(generic.sendIntervalMinutes == tracker_pi::kDefaultSendIntervalMinutes,
         "generic behavior should clamp the interval");
  expect(generic.minDistanceMeters == tracker_pi::kDefaultMinDistanceMeters,
         "generic behavior should default the minimum distance");

  const auto genericUi = tracker_pi::getEndpointTypeBehavior(generic).uiMetadata();
  expect(genericUi.showsGenericTransportFields,
         "generic behavior should show transport fields");
  expect(genericUi.supportsAwaAws,
         "generic behavior should support AWA/AWS");

  tracker_pi::EndpointConfig nflEndpoint;
  nflEndpoint.type = tracker_pi::kEndpointTypeNoForeignLand;
  nflEndpoint.sendIntervalMinutes = 1;
  tracker_pi::getEndpointTypeBehavior(nflEndpoint).applyDefaults(nflEndpoint);
  expect(nflEndpoint.sendIntervalMinutes == tracker_pi::kNflMinSendIntervalMinutes,
         "NFL behavior should clamp the interval");
  expect(nflEndpoint.minDistanceMeters == tracker_pi::nfl::kMinDistanceMeters,
         "NFL behavior should fix the minimum distance");
  expect(!nflEndpoint.includeAwaAws, "NFL behavior should disable AWA/AWS");
  expect(nflEndpoint.headerName == "X-NFL-API-Key",
         "NFL behavior should set the header name");

  const auto nflUi = tracker_pi::getEndpointTypeBehavior(nflEndpoint).uiMetadata();
  expect(!nflUi.showsGenericTransportFields,
         "NFL behavior should hide generic transport fields");
  expect(!nflUi.supportsAwaAws,
         "NFL behavior should disable AWA/AWS support");
  expect(nflUi.headerValueLabel == "My NFL boat key",
         "NFL behavior should expose the boat key label");

  const auto missingKey =
      tracker_pi::getEndpointTypeBehavior(nflEndpoint).validate(nflEndpoint);
  expect(missingKey.has_value() && *missingKey == "NFL boat key is required",
         "NFL validation should require a boat key");

  nflEndpoint.headerValue = "424534f5-13bc-42e8-ad02-33f9e27f7750";
  const auto validKey =
      tracker_pi::getEndpointTypeBehavior(nflEndpoint).validate(nflEndpoint);
  expect(!validKey.has_value(), "valid NFL boat key should pass validation");

  return EXIT_SUCCESS;
}
