#include "1tracker_pi/config_loader.h"
#include "1tracker_pi/endpoint_policy.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <json/json.h>

namespace tracker_pi {
namespace {

std::string readFile(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    throw std::runtime_error("Unable to open config file: " + path.string());
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

bool requireBool(const Json::Value& value, const char* field) {
  if (!value.isMember(field) || !value[field].isBool()) {
    throw std::runtime_error(std::string("Config field must be boolean: ") + field);
  }
  return value[field].asBool();
}

int requirePositiveInt(const Json::Value& value, const char* field) {
  if (!value.isMember(field) || !value[field].isInt()) {
    throw std::runtime_error(std::string("Config field must be integer: ") + field);
  }

  const int parsed = value[field].asInt();
  if (parsed <= 0) {
    throw std::runtime_error(std::string("Config field must be > 0: ") + field);
  }
  return parsed;
}

int requireNonNegativeInt(const Json::Value& value, const char* field) {
  if (!value.isMember(field) || !value[field].isInt()) {
    throw std::runtime_error(std::string("Config field must be integer: ") + field);
  }

  const int parsed = value[field].asInt();
  if (parsed < 0) {
    throw std::runtime_error(std::string("Config field must be >= 0: ") + field);
  }
  return parsed;
}

std::string requireString(const Json::Value& value, const char* field) {
  if (!value.isMember(field) || !value[field].isString()) {
    throw std::runtime_error(std::string("Config field must be string: ") + field);
  }

  const std::string parsed = value[field].asString();
  if (parsed.empty()) {
    throw std::runtime_error(std::string("Config field must not be empty: ") + field);
  }
  return parsed;
}

std::string requireStringAllowEmpty(const Json::Value& value, const char* field) {
  if (!value.isMember(field) || !value[field].isString()) {
    throw std::runtime_error(std::string("Config field must be string: ") + field);
  }
  return value[field].asString();
}

void parseEndpointIdentity(EndpointConfig& endpoint, const Json::Value& value) {
  if (value.isMember("id")) {
    endpoint.id = requireStringAllowEmpty(value, "id");
  }
  endpoint.name = requireString(value, "name");
  endpoint.enabled = requireBool(value, "enabled");
  if (value.isMember("type")) {
    endpoint.type = requireString(value, "type");
  }
  if (endpoint.type != kEndpointTypeHttpJsonWithHeaderKey &&
      endpoint.type != kEndpointTypeNoForeignLand) {
    throw std::runtime_error("Unsupported endpoint type: " + endpoint.type);
  }
}

void parseEndpointBehavior(EndpointConfig& endpoint, const Json::Value& value,
                           int defaultIntervalMinutes) {
  if (value.isMember("includeAwaAws")) {
    endpoint.includeAwaAws = requireBool(value, "includeAwaAws");
  }
  if (value.isMember("sendIntervalMinutes")) {
    endpoint.sendIntervalMinutes = requirePositiveInt(value, "sendIntervalMinutes");
  } else {
    endpoint.sendIntervalMinutes =
        isNoForeignLandEndpoint(endpoint) ? kNflDefaultSendIntervalMinutes
                                          : std::max(1, defaultIntervalMinutes);
  }
  if (value.isMember("minDistanceMeters")) {
    endpoint.minDistanceMeters = requireNonNegativeInt(value, "minDistanceMeters");
  } else {
    endpoint.minDistanceMeters = effectiveMinDistanceMeters(endpoint);
  }
}

void parseEndpointTransport(EndpointConfig& endpoint, const Json::Value& value) {
  endpoint.url = requireStringAllowEmpty(value, "url");
  endpoint.timeoutSeconds = requirePositiveInt(value, "timeoutSeconds");
  endpoint.headerName = requireStringAllowEmpty(value, "headerName");
  endpoint.headerValue = requireStringAllowEmpty(value, "headerValue");
}

EndpointConfig parseEndpoint(const Json::Value& value, int defaultIntervalMinutes) {
  EndpointConfig endpoint;
  parseEndpointIdentity(endpoint, value);
  parseEndpointBehavior(endpoint, value, defaultIntervalMinutes);
  parseEndpointTransport(endpoint, value);
  normalizeEndpointConfig(endpoint);
  return endpoint;
}

Json::Value parseConfigRoot(const std::string& jsonText) {
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;

  Json::Value root;
  std::string errors;
  std::istringstream input(jsonText);
  if (!Json::parseFromStream(builder, input, &root, &errors)) {
    throw std::runtime_error("Invalid config JSON: " + errors);
  }

  if (!root.isObject()) {
    throw std::runtime_error("Config root must be an object");
  }

  return root;
}

int parseDefaultIntervalMinutes(const Json::Value& root) {
  if (root.isMember("sendIntervalSeconds") && root["sendIntervalSeconds"].isInt()) {
    return std::max(1, (root["sendIntervalSeconds"].asInt() + 59) / 60);
  }
  return 1;
}

void parseEndpointsArray(RuntimeConfig& config, const Json::Value& root,
                         int defaultIntervalMinutes) {
  if (!root.isMember("endpoints") || !root["endpoints"].isArray()) {
    throw std::runtime_error("Config field must be an array: endpoints");
  }

  for (const Json::Value& endpointValue : root["endpoints"]) {
    config.endpoints.push_back(parseEndpoint(endpointValue, defaultIntervalMinutes));
  }
}

}  // namespace

RuntimeConfig ConfigLoader::loadFromFile(const std::filesystem::path& path) const {
  return loadFromString(readFile(path));
}

RuntimeConfig ConfigLoader::loadFromString(const std::string& jsonText) const {
  const Json::Value root = parseConfigRoot(jsonText);

  RuntimeConfig config;
  config.enabled = requireBool(root, "enabled");
  const int defaultIntervalMinutes = parseDefaultIntervalMinutes(root);
  parseEndpointsArray(config, root, defaultIntervalMinutes);

  normalizeRuntimeConfig(config);
  return config;
}

}  // namespace tracker_pi
