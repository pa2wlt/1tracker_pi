#pragma once

#include <string>

namespace tracker_pi {

struct EndpointConfig {
  std::string name;
  std::string type = "http_json_with_header_key";
  bool enabled = true;
  bool includeAwaAws = true;
  int sendIntervalMinutes = 1;
  int minDistanceMeters = 60;
  std::string url;
  int timeoutSeconds = 10;
  std::string headerName;
  std::string headerValue;
  std::string id;
};

}  // namespace tracker_pi
