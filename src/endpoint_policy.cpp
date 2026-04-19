#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/nfl_settings.h"
#include "1tracker_pi/endpoint_type_behavior.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <regex>
#include <sstream>

namespace tracker_pi {

bool isNoForeignLandType(const std::string& type) {
  return type == kEndpointTypeNoForeignLand;
}

bool isNoForeignLandEndpoint(const EndpointConfig& endpoint) {
  return isNoForeignLandType(endpoint.type);
}

bool isValidNflBoatKey(const std::string& value) {
  static const std::regex pattern(
      "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
  return std::regex_match(value, pattern);
}

std::string makeEndpointId() {
  static std::atomic<unsigned long long> counter{0};
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto nonce = counter.fetch_add(1, std::memory_order_relaxed);

  std::ostringstream stream;
  stream << "endpoint-" << std::hex << now << "-" << nonce;
  return stream.str();
}

std::string endpointStateKey(const EndpointConfig& endpoint) {
  return endpoint.id.empty() ? endpoint.name : endpoint.id;
}

int effectiveSendIntervalMinutes(const EndpointConfig& endpoint) {
  const int minimum = isNoForeignLandEndpoint(endpoint)
                          ? kNflMinSendIntervalMinutes
                          : kDefaultSendIntervalMinutes;
  return std::max(minimum, endpoint.sendIntervalMinutes);
}

int effectiveMinDistanceMeters(const EndpointConfig& endpoint) {
  if (isNoForeignLandEndpoint(endpoint)) {
    return nfl::kMinDistanceMeters;
  }
  return std::max(0, endpoint.minDistanceMeters);
}

void normalizeEndpointConfig(EndpointConfig& endpoint) {
  endpoint.sendIntervalMinutes = effectiveSendIntervalMinutes(endpoint);
  endpoint.minDistanceMeters = effectiveMinDistanceMeters(endpoint);
  getEndpointTypeBehavior(endpoint).applyDefaults(endpoint);
}

void normalizeRuntimeConfig(RuntimeConfig& config) {
  for (auto& endpoint : config.endpoints) {
    if (endpoint.id.empty()) {
      endpoint.id = makeEndpointId();
    }
    normalizeEndpointConfig(endpoint);
  }
}

bool validateRuntimeConfig(const RuntimeConfig& config, std::string* errorMessage) {
  for (const auto& endpoint : config.endpoints) {
    if (isNoForeignLandEndpoint(endpoint) &&
        !isValidNflBoatKey(endpoint.headerValue)) {
      if (errorMessage != nullptr) {
        *errorMessage =
            "NFL boat key must be a UUID like "
            "424534f5-13bc-42e8-ad02-33f9e27f7750.";
      }
      return false;
    }
  }
  return true;
}

std::optional<std::string> validateEndpointForSend(const EndpointConfig& endpoint) {
  return getEndpointTypeBehavior(endpoint).validate(endpoint);
}

std::string makeNflEndpointName(std::size_t index) {
  return "nfl-" + std::to_string(index + 1);
}

}  // namespace tracker_pi
