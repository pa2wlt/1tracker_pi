#include <cstdlib>
#include <stdexcept>
#include <string>

#include "1tracker_pi/endpoint_policy.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  tracker_pi::RuntimeConfig config;
  config.endpoints.push_back(
      {"main", tracker_pi::kEndpointTypeHttpJsonWithHeaderKey, true, true, 1, 60,
       "https://example.com", 10, "X-API-Key", "SECRET"});
  config.endpoints.push_back(
      {"secondary", tracker_pi::kEndpointTypeHttpJsonWithHeaderKey, true, true,
       1, 60, "https://example.com/2", 10, "X-API-Key", "SECRET", "endpoint-2"});

  tracker_pi::normalizeRuntimeConfig(config);

  expect(!config.endpoints.front().id.empty(),
         "normalizeRuntimeConfig should assign ids to legacy endpoints");
  expect(config.endpoints.back().id == "endpoint-2",
         "normalizeRuntimeConfig should preserve existing ids");
  expect(tracker_pi::endpointStateKey(config.endpoints.front()) ==
             config.endpoints.front().id,
         "endpointStateKey should prefer the stable endpoint id");

  tracker_pi::EndpointConfig unnamed;
  unnamed.name = "fallback";
  expect(tracker_pi::endpointStateKey(unnamed) == "fallback",
         "endpointStateKey should fall back to the endpoint name");

  return EXIT_SUCCESS;
}
