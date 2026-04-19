#include <cstdlib>
#include <stdexcept>
#include <string>

#include "1tracker_pi/endpoint_config.h"
#include "1tracker_pi/endpoint_sender.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  expect(tracker_pi::EndpointSender::MaskSecret("abcd") == "****",
         "short secret should be fully masked");
  expect(tracker_pi::EndpointSender::MaskSecret("12345678") == "****5678",
         "long secret should expose last four chars only");

  tracker_pi::EndpointConfig endpoint;
  endpoint.headerValue = "SECRET1234";
  const auto redacted = tracker_pi::EndpointSender::RedactSensitiveText(
      "header=SECRET1234 response=SECRET1234", endpoint);
  expect(redacted == "header=******1234 response=******1234",
         "secret occurrences should be redacted in arbitrary text");

  return EXIT_SUCCESS;
}
