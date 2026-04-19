#pragma once

#include <optional>
#include <string>
#include <vector>

#include "1tracker_pi/endpoint_config.h"
#include "1tracker_pi/snapshot.h"

namespace tracker_pi {

struct EndpointUiMetadata {
  std::string headerValueLabel = "Value";
  std::string headerValueTooltip;
  bool showsGenericTransportFields = true;
  bool supportsAwaAws = true;
};

struct EndpointRequest {
  std::string contentType;
  std::vector<std::string> headers;
  std::string body;
};

class EndpointTypeBehavior {
public:
  virtual ~EndpointTypeBehavior() = default;

  virtual const char* type() const = 0;
  virtual void applyDefaults(EndpointConfig& endpoint) const = 0;
  virtual std::optional<std::string> validate(const EndpointConfig& endpoint) const = 0;
  virtual EndpointUiMetadata uiMetadata() const = 0;
  virtual std::optional<std::string> buildPayload(
      const Snapshot& snapshot, const EndpointConfig& endpoint) const = 0;
  virtual EndpointRequest buildRequest(const EndpointConfig& endpoint,
                                       const std::string& payload) const = 0;
  virtual bool responseIndicatesSuccess(long httpStatus,
                                        const std::string& responseBody) const = 0;
};

const EndpointTypeBehavior& getEndpointTypeBehavior(const EndpointConfig& endpoint);
const EndpointTypeBehavior& getEndpointTypeBehavior(const std::string& type);
std::vector<std::string> listEndpointTypes();

}  // namespace tracker_pi
