#pragma once

namespace tracker_pi::nfl {

enum class Environment {
  kPrd,
  kUat,
};

inline constexpr int kTimeoutSeconds = 10;
inline constexpr int kMinDistanceMeters = 60;
inline constexpr const char* kRestApiHeaderName = "X-NFL-API-Key";
inline constexpr Environment kEnvironment = Environment::kPrd;

inline constexpr const char* kProdHost = "www.noforeignland.com";
inline constexpr const char* kProdRestApiKey =
    "f7685cef-71a6-431d-88f6-a8a4ea402c2e";

inline constexpr const char* kUatHost = "www.noforeignland-uat.com";
inline constexpr const char* kUatRestApiKey =
    "00000000-0000-0000-0000-000000000000";

inline constexpr const char* host() {
  return kEnvironment == Environment::kPrd ? kProdHost : kUatHost;
}

inline constexpr const char* trackingUrl() {
  return kEnvironment == Environment::kPrd
             ? "https://www.noforeignland.com/api/v1/boat/tracking/track"
             : "https://www.noforeignland-uat.com/api/v1/boat/tracking/track";
}

inline constexpr const char* boatTrackingSettingsUrl() {
  return kEnvironment == Environment::kPrd
             ? "https://www.noforeignland.com/map/settings/boat/tracking/api"
             : "https://www.noforeignland-uat.com/map/settings/boat/tracking/api";
}

inline constexpr const char* restApiKey() {
  return kEnvironment == Environment::kPrd ? kProdRestApiKey : kUatRestApiKey;
}

}  // namespace tracker_pi::nfl
