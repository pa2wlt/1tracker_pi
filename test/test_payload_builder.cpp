#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <json/json.h>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/payload_builder.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  tracker_pi::Snapshot snapshot;
  snapshot.timevalue = 1710000000;
  snapshot.lat = 52.12345;
  snapshot.lon = 4.98765;
  snapshot.awa = 38.2;
  snapshot.aws = 14.6;
  tracker_pi::EndpointConfig endpoint;
  endpoint.includeAwaAws = true;

  tracker_pi::PayloadBuilder builder;
  const auto payload = builder.buildPayload(snapshot, endpoint);

  expect(payload.has_value(), "payload should be generated");

  Json::CharReaderBuilder builderConfig;
  Json::Value root;
  std::string errors;
  std::istringstream input(*payload);
  expect(Json::parseFromStream(builderConfig, input, &root, &errors),
         "payload should be valid JSON");

  expect(root.isObject(), "payload root should be an object");
  expect(root["action"].asString() == "addPositions", "missing action");
  expect(root["data"].isArray(), "data should be an array");
  expect(root["data"].size() == 1, "payload should contain one snapshot");

  const Json::Value& first = root["data"][0];
  expect(first["timevalue"].asInt64() == 1710000000, "missing timevalue");
  expect(first["lat"].asDouble() > 52.12 && first["lat"].asDouble() < 52.13,
         "missing lat");
  expect(first["aws"].asDouble() == 14.6, "missing aws");

  endpoint.includeAwaAws = false;
  const auto payloadWithoutWind = builder.buildPayload(snapshot, endpoint);
  expect(payloadWithoutWind.has_value(), "payload without wind should be generated");

  Json::Value rootWithoutWind;
  std::istringstream inputWithoutWind(*payloadWithoutWind);
  expect(Json::parseFromStream(builderConfig, inputWithoutWind, &rootWithoutWind,
                               &errors),
         "payload without wind should be valid JSON");
  const Json::Value& second = rootWithoutWind["data"][0];
  expect(!second.isMember("awa"), "awa should be omitted");
  expect(!second.isMember("aws"), "aws should be omitted");

  endpoint.type = tracker_pi::kEndpointTypeNoForeignLand;
  endpoint.headerValue = "424534f5-13bc-42e8-ad02-33f9e27f7750";
  const auto nflPayload = builder.buildPayload(snapshot, endpoint);
  expect(nflPayload.has_value(), "NFL payload should be generated");

  Json::Value nflTrack;
  std::istringstream nflInput(*nflPayload);
  expect(Json::parseFromStream(builderConfig, nflInput, &nflTrack, &errors),
         "NFL payload should be valid JSON");
  expect(nflTrack.isArray(), "NFL payload should be an array");
  expect(nflTrack.size() == 1, "NFL payload should contain one point");
  expect(nflTrack[0].isArray(), "NFL point should be an array");
  expect(nflTrack[0][0].asInt64() == 1710000000000LL,
         "NFL timestamp should be in milliseconds");
  expect(nflTrack[0][1].asDouble() > 52.12 && nflTrack[0][1].asDouble() < 52.13,
         "NFL lat missing");
  expect(nflTrack[0][2].asDouble() > 4.98 && nflTrack[0][2].asDouble() < 4.99,
         "NFL lon missing");

  return EXIT_SUCCESS;
}
