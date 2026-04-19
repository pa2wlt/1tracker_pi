#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "1tracker_pi/config_loader.h"
#include "1tracker_pi/nfl_settings.h"

namespace {

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  const std::string json = R"({
    "enabled": true,
    "endpoints": [{
      "id": "endpoint-main",
      "name": "main",
      "type": "http_json_with_header_key",
      "enabled": true,
      "includeAwaAws": false,
      "sendIntervalMinutes": 5,
      "minDistanceMeters": 42,
      "url": "https://example.com/api/position",
      "timeoutSeconds": 10,
      "headerName": "X-API-Key",
      "headerValue": "SECRET"
    }, {
      "name": "nfl",
      "type": "noforeignland",
      "enabled": true,
      "includeAwaAws": true,
      "sendIntervalMinutes": 5,
      "url": "",
      "timeoutSeconds": 10,
      "headerName": "",
      "headerValue": ""
    }]
  })";

  tracker_pi::ConfigLoader loader;
  const auto config = loader.loadFromString(json);

  expect(config.enabled, "config should be enabled");
  expect(config.endpoints.size() == 2, "expected exactly two endpoints");
  expect(config.endpoints.front().id == "endpoint-main",
         "explicit endpoint id should be preserved");
  expect(config.endpoints.front().type == "http_json_with_header_key",
         "unexpected endpoint type");
  expect(!config.endpoints.front().includeAwaAws,
         "unexpected includeAwaAws value");
  expect(config.endpoints.front().sendIntervalMinutes == 5,
         "unexpected endpoint interval");
  expect(config.endpoints.front().minDistanceMeters == 42,
         "unexpected endpoint minimum distance");
  expect(config.endpoints.back().type == "noforeignland",
         "unexpected noforeignland endpoint type");
  expect(!config.endpoints.back().id.empty(),
         "loader should generate an id for legacy endpoint configs");
  expect(config.endpoints.back().sendIntervalMinutes == 10,
         "NFL interval should be clamped to 10 minutes");
  expect(config.endpoints.back().minDistanceMeters == 60,
         "NFL minimum distance should be fixed to 60 meters");
  expect(config.endpoints.back().headerName == "X-NFL-API-Key",
         "NFL header name should be normalized");
  expect(config.endpoints.back().url == tracker_pi::nfl::trackingUrl(),
         "NFL URL should be normalized");

  return EXIT_SUCCESS;
}
