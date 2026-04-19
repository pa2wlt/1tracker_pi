#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "1tracker_pi/endpoint_config.h"
#include "1tracker_pi/runtime_config.h"

namespace tracker_pi {

inline constexpr const char* kEndpointTypeHttpJsonWithHeaderKey =
    "http_json_with_header_key";
inline constexpr const char* kEndpointTypeNoForeignLand = "noforeignland";
inline constexpr int kDefaultSendIntervalMinutes = 1;
inline constexpr int kDefaultMinDistanceMeters = 60;
inline constexpr int kNflDefaultSendIntervalMinutes = 15;
inline constexpr int kNflMinSendIntervalMinutes = 10;

bool isNoForeignLandType(const std::string& type);
bool isNoForeignLandEndpoint(const EndpointConfig& endpoint);
bool isValidNflBoatKey(const std::string& value);
std::string makeEndpointId();
std::string endpointStateKey(const EndpointConfig& endpoint);
int effectiveSendIntervalMinutes(const EndpointConfig& endpoint);
int effectiveMinDistanceMeters(const EndpointConfig& endpoint);
void normalizeEndpointConfig(EndpointConfig& endpoint);
void normalizeRuntimeConfig(RuntimeConfig& config);
bool validateRuntimeConfig(const RuntimeConfig& config, std::string* errorMessage);
std::optional<std::string> validateEndpointForSend(const EndpointConfig& endpoint);
std::string makeNflEndpointName(std::size_t index);

}  // namespace tracker_pi
