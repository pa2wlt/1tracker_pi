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

  // Force libcurl + OpenSSL initialisation on the calling thread. Call
  // once from the OpenCPN main thread during plugin Init(), before the
  // scheduler worker thread starts. Without this, the first send() ticks
  // on the worker thread and triggers TLS-slot allocation + OpenSSL
  // lazy init from a thread that didn't exist when the plugin .so was
  // dlopen'd — a known crash mode on Android.
  static void Prewarm();
};

}  // namespace tracker_pi
