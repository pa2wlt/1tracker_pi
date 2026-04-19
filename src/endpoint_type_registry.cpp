#include "1tracker_pi/endpoint_type_behavior.h"

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/nfl_settings.h"

#include <json/json.h>

#include <curl/curl.h>

#include <algorithm>
#include <sstream>

namespace tracker_pi {
namespace {

class GenericJsonBehavior final : public EndpointTypeBehavior {
public:
  const char* type() const override { return kEndpointTypeHttpJsonWithHeaderKey; }

  void applyDefaults(EndpointConfig& endpoint) const override {
    endpoint.sendIntervalMinutes =
        std::max(kDefaultSendIntervalMinutes, endpoint.sendIntervalMinutes);
    endpoint.minDistanceMeters = std::max(0, endpoint.minDistanceMeters);
  }

  std::optional<std::string> validate(const EndpointConfig& endpoint) const override {
    (void)endpoint;
    return std::nullopt;
  }

  EndpointUiMetadata uiMetadata() const override { return EndpointUiMetadata{}; }

  std::optional<std::string> buildPayload(
      const Snapshot& snapshot, const EndpointConfig& endpoint) const override {
    if (!snapshot.hasValidPosition() || !snapshot.hasTimestamp()) {
      return std::nullopt;
    }

    Json::Value item(Json::objectValue);
    item["timevalue"] = Json::Int64(*snapshot.timevalue);
    item["lat"] = *snapshot.lat;
    item["lon"] = *snapshot.lon;

    if (endpoint.includeAwaAws && snapshot.awa.has_value()) {
      item["awa"] = *snapshot.awa;
    }

    if (endpoint.includeAwaAws && snapshot.aws.has_value()) {
      item["aws"] = *snapshot.aws;
    }

    Json::Value data(Json::arrayValue);
    data.append(item);

    Json::Value root(Json::objectValue);
    root["action"] = "addPositions";
    root["data"] = data;

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    return Json::writeString(writerBuilder, root);
  }

  EndpointRequest buildRequest(const EndpointConfig& endpoint,
                               const std::string& payload) const override {
    return EndpointRequest{
        "application/json",
        {endpoint.headerName + ": " + endpoint.headerValue},
        payload,
    };
  }

  bool responseIndicatesSuccess(long httpStatus,
                                const std::string& responseBody) const override {
    (void)responseBody;
    return httpStatus >= 200 && httpStatus < 300;
  }
};

class NoForeignLandBehavior final : public EndpointTypeBehavior {
public:
  const char* type() const override { return kEndpointTypeNoForeignLand; }

  void applyDefaults(EndpointConfig& endpoint) const override {
    endpoint.sendIntervalMinutes =
        std::max(kNflMinSendIntervalMinutes, endpoint.sendIntervalMinutes);
    endpoint.minDistanceMeters = nfl::kMinDistanceMeters;
    endpoint.includeAwaAws = false;
    if (endpoint.url.empty()) {
      endpoint.url = nfl::trackingUrl();
    }
    if (endpoint.timeoutSeconds <= 0) {
      endpoint.timeoutSeconds = nfl::kTimeoutSeconds;
    }
    if (endpoint.headerName.empty()) {
      endpoint.headerName = nfl::kRestApiHeaderName;
    }
  }

  std::optional<std::string> validate(const EndpointConfig& endpoint) const override {
    if (endpoint.headerValue.empty()) {
      return std::string("NFL boat key is required");
    }
    if (!isValidNflBoatKey(endpoint.headerValue)) {
      return std::string("NFL boat key format is invalid");
    }
    return std::nullopt;
  }

  EndpointUiMetadata uiMetadata() const override {
    EndpointUiMetadata metadata;
    metadata.headerValueLabel = "My NFL boat key";
    metadata.headerValueTooltip =
        "Create your own in NFL via Boat Tracking Settings > API Key on "
        + std::string(nfl::boatTrackingSettingsUrl());
    metadata.showsGenericTransportFields = false;
    metadata.supportsAwaAws = false;
    return metadata;
  }

  std::optional<std::string> buildPayload(
      const Snapshot& snapshot, const EndpointConfig& endpoint) const override {
    (void)endpoint;
    if (!snapshot.hasValidPosition() || !snapshot.hasTimestamp()) {
      return std::nullopt;
    }

    const std::int64_t timestamp =
        *snapshot.timevalue < 100000000000LL ? *snapshot.timevalue * 1000
                                             : *snapshot.timevalue;

    Json::Value point(Json::arrayValue);
    point.append(Json::Int64(timestamp));
    point.append(*snapshot.lat);
    point.append(*snapshot.lon);

    Json::Value track(Json::arrayValue);
    track.append(point);

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    return Json::writeString(writerBuilder, track);
  }

  EndpointRequest buildRequest(const EndpointConfig& endpoint,
                               const std::string& payload) const override {
    CURL* handle = curl_easy_init();
    std::string encodedBoatKey = endpoint.headerValue;
    std::string encodedTrack = payload;
    if (handle != nullptr) {
      if (char* escaped = curl_easy_escape(handle, endpoint.headerValue.c_str(),
                                           static_cast<int>(endpoint.headerValue.size()));
          escaped != nullptr) {
        encodedBoatKey = escaped;
        curl_free(escaped);
      }
      if (char* escaped =
              curl_easy_escape(handle, payload.c_str(), static_cast<int>(payload.size()));
          escaped != nullptr) {
        encodedTrack = escaped;
        curl_free(escaped);
      }
      curl_easy_cleanup(handle);
    }

    return EndpointRequest{
        "application/x-www-form-urlencoded",
        {std::string(nfl::kRestApiHeaderName) + ": " + nfl::restApiKey()},
        "boatApiKey=" + encodedBoatKey + "&track=" + encodedTrack,
    };
  }

  bool responseIndicatesSuccess(long httpStatus,
                                const std::string& responseBody) const override {
    if (httpStatus < 200 || httpStatus >= 300) {
      return false;
    }
    if (responseBody.empty()) {
      return true;
    }

    Json::CharReaderBuilder builder;
    std::string errors;
    Json::Value root;
    std::istringstream input(responseBody);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
      return true;
    }

    const Json::Value status = root["status"];
    if (!status.isString()) {
      return true;
    }

    const std::string statusValue = status.asString();
    return statusValue == "ok" || statusValue == "success";
  }
};

const GenericJsonBehavior kGenericJsonBehavior;
const NoForeignLandBehavior kNoForeignLandBehavior;

const EndpointTypeBehavior* findBehavior(const std::string& type) {
  if (type == kEndpointTypeNoForeignLand) {
    return &kNoForeignLandBehavior;
  }
  return &kGenericJsonBehavior;
}

}  // namespace

const EndpointTypeBehavior& getEndpointTypeBehavior(const EndpointConfig& endpoint) {
  return *findBehavior(endpoint.type);
}

const EndpointTypeBehavior& getEndpointTypeBehavior(const std::string& type) {
  return *findBehavior(type);
}

std::vector<std::string> listEndpointTypes() {
  return {kEndpointTypeHttpJsonWithHeaderKey, kEndpointTypeNoForeignLand};
}

}  // namespace tracker_pi
