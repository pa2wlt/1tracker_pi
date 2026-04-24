#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>

#include "1tracker_pi/endpoint_config.h"
#include "1tracker_pi/endpoint_error_summary.h"
#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/endpoint_ui_state.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

tracker_pi::EndpointConfig jsonEndpoint() {
  tracker_pi::EndpointConfig endpoint;
  endpoint.id = "endpoint-json-1";
  endpoint.name = "my-json";
  endpoint.type = tracker_pi::kEndpointTypeHttpJsonWithHeaderKey;
  endpoint.url = "https://example.test/ingest";
  return endpoint;
}

tracker_pi::EndpointConfig nflEndpoint() {
  tracker_pi::EndpointConfig endpoint;
  endpoint.id = "endpoint-nfl-1";
  endpoint.name = "my-nfl";
  endpoint.type = tracker_pi::kEndpointTypeNoForeignLand;
  return endpoint;
}

}  // namespace

int main() {
  using tracker_pi::summarizeEndpointError;
  const auto json = jsonEndpoint();
  const auto nfl = nflEndpoint();

  // Empty raw error yields empty summary
  expect(summarizeEndpointError(json, "") == "", "empty raw should map to empty");

  // Missing API key — distinct messages for NFL vs JSON
  expect(summarizeEndpointError(json, "server: an active API key was not provided") ==
             "The API key was not accepted by the server.",
         "json missing key");
  expect(summarizeEndpointError(nfl, "an active API key was not provided here") ==
             "Your NFL boat key is missing or invalid.",
         "nfl missing key");

  // Invalid/expired token
  expect(summarizeEndpointError(json, "body=Invalid or expired token") ==
             "The authorization token is invalid or expired.",
         "invalid token");

  // DNS failure
  expect(summarizeEndpointError(json, "Could not resolve host: example.invalid") ==
             "The server name could not be resolved.",
         "dns failure");

  // Connect failures (both phrases)
  expect(summarizeEndpointError(json, "curl: Failed to connect to host") ==
             "Could not connect to the server.",
         "failed to connect");
  expect(summarizeEndpointError(json, "Connection refused") ==
             "Could not connect to the server.",
         "connection refused");

  // Timeout
  expect(summarizeEndpointError(json, "Operation timed out after 30s") ==
             "The server took too long to respond.",
         "timeout");

  // HTTP status codes
  expect(summarizeEndpointError(json, "HTTP 401 Unauthorized") ==
             "The server rejected the credentials.",
         "http 401");
  expect(summarizeEndpointError(json, "HTTP 403 Forbidden") ==
             "The server refused this request.",
         "http 403");
  expect(summarizeEndpointError(json, "HTTP 404 Not Found") ==
             "The tracking URL could not be found.",
         "http 404");
  expect(summarizeEndpointError(json, "HTTP 200 but body=bad") ==
             "The server responded, but rejected the tracking data.",
         "http 200 rejected");
  expect(summarizeEndpointError(json, "HTTP 503 Service Unavailable") ==
             "The server returned an error while processing the tracking request.",
         "http 5xx generic");

  // Unknown raw error falls back to generic message
  expect(summarizeEndpointError(json, "something completely unrecognized") ==
             "Tracking could not be sent. Check the technical details below.",
         "generic fallback");

  // First matching rule wins — DNS phrase inside a larger error still maps to DNS
  expect(summarizeEndpointError(json, "Could not resolve host and also HTTP 500") ==
             "The server name could not be resolved.",
         "first match wins");

  // computeEndpointErrorUiState: pure UI state decision
  using tracker_pi::computeEndpointErrorUiState;
  using tracker_pi::EndpointErrorUiState;
  using tracker_pi::EndpointUiState;
  using tracker_pi::endpointStateKey;
  std::map<std::string, EndpointUiState> statuses;

  // No endpoint → invisible
  const auto stNullEndpoint = computeEndpointErrorUiState(nullptr, statuses);
  expect(!stNullEndpoint.visible, "null endpoint must be invisible");

  // Endpoint without a recorded status → invisible
  const auto stNoStatus = computeEndpointErrorUiState(&json, statuses);
  expect(!stNoStatus.visible, "missing status must be invisible");

  // Endpoint with empty error → invisible
  EndpointUiState empty;
  empty.lastSentLocalTime = "2026-04-22 10:00";
  statuses[endpointStateKey(json)] = empty;
  const auto stNoError = computeEndpointErrorUiState(&json, statuses);
  expect(!stNoError.visible, "empty error must be invisible");

  // Endpoint with error + timestamp → full UI state
  EndpointUiState failing;
  failing.lastErrorMessage = "HTTP 401 Unauthorized";
  failing.lastSentLocalTime = "2026-04-22 12:34";
  statuses[endpointStateKey(json)] = failing;
  const auto stFail = computeEndpointErrorUiState(&json, statuses);
  expect(stFail.visible, "failing endpoint must be visible");
  expect(stFail.details == "HTTP 401 Unauthorized", "details must be raw error");
  expect(stFail.summary == "The server rejected the credentials.",
         "summary must come from summarizeEndpointError");
  expect(stFail.lastSentLocalTime == "2026-04-22 12:34",
         "lastSentLocalTime must pass through the timestamp "
         "(UI layer adds the \"Last call: \" prefix so it can be translated)");

  // Endpoint with error but no timestamp → visible without meta
  EndpointUiState failingNoTs;
  failingNoTs.lastErrorMessage = "Connection refused by server";
  statuses[endpointStateKey(json)] = failingNoTs;
  const auto stFailNoTs = computeEndpointErrorUiState(&json, statuses);
  expect(stFailNoTs.visible, "failing without timestamp still visible");
  expect(stFailNoTs.lastSentLocalTime.empty(),
         "lastSentLocalTime empty when no timestamp recorded");

  // NFL missing-key phrase gets NFL-specific translation
  statuses.clear();
  EndpointUiState nflFail;
  nflFail.lastErrorMessage = "response: an active API key was not provided";
  nflFail.lastSentLocalTime = "2026-04-22 13:00";
  statuses[endpointStateKey(nfl)] = nflFail;
  const auto stNfl = computeEndpointErrorUiState(&nfl, statuses);
  expect(stNfl.visible, "nfl failure visible");
  expect(stNfl.summary == "Your NFL boat key is missing or invalid.",
         "nfl-specific key message");

  return EXIT_SUCCESS;
}
