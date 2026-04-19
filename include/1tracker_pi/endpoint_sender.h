#pragma once

#include <string>

#include "1tracker_pi/endpoint_config.h"

namespace tracker_pi {

class EndpointSender {
public:
  virtual ~EndpointSender() = default;

  struct Result {
    bool success = false;
    long httpStatus = 0;
    std::string message;
  };

  virtual Result send(const EndpointConfig& endpoint,
                      const std::string& payload) const;

  static std::string MaskSecret(const std::string& secret);
  static std::string RedactSensitiveText(std::string text,
                                         const EndpointConfig& endpoint);
};

}  // namespace tracker_pi
