#include "1tracker_pi/endpoint_error_summary.h"

#include <array>
#include <string_view>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/translation_markers.h"

namespace tracker_pi {

namespace {

bool containsText(const std::string& text, std::string_view needle) {
  return text.find(needle) != std::string::npos;
}

struct ErrorPattern {
  std::string_view needle;    // matched against the raw (English) libcurl
                              // error text — not user-facing, not translated
  std::string_view message;   // shown to the user; marked with TR_NOOP so
                              // xgettext extracts it into the .pot file
};

// Ordered: first match wins. Each message is marked TR_NOOP; the UI layer
// passes it through wxGetTranslation at display time.
constexpr std::array<ErrorPattern, 9> kPatterns{{
    {"Invalid or expired token", TR_NOOP("The authorization token is invalid or expired.")},
    {"Could not resolve host",   TR_NOOP("The server name could not be resolved.")},
    {"Failed to connect",        TR_NOOP("Could not connect to the server.")},
    {"Connection refused",       TR_NOOP("Could not connect to the server.")},
    {"timed out",                TR_NOOP("The server took too long to respond.")},
    {"HTTP 401",                 TR_NOOP("The server rejected the credentials.")},
    {"HTTP 403",                 TR_NOOP("The server refused this request.")},
    {"HTTP 404",                 TR_NOOP("The tracking URL could not be found.")},
    {"HTTP 200",                 TR_NOOP("The server responded, but rejected the tracking data.")},
}};

constexpr std::string_view kMissingApiKeyNeedle =
    "an active API key was not provided";
constexpr std::string_view kGenericHttpNeedle = "HTTP ";
constexpr std::string_view kMissingApiKeyNfl =
    TR_NOOP("Your NFL boat key is missing or invalid.");
constexpr std::string_view kMissingApiKeyJson =
    TR_NOOP("The API key was not accepted by the server.");
constexpr std::string_view kGenericHttpMessage =
    TR_NOOP("The server returned an error while processing the tracking request.");
constexpr std::string_view kFallbackMessage =
    TR_NOOP("Tracking could not be sent. Check the technical details below.");

}  // namespace

std::string summarizeEndpointError(const EndpointConfig& endpoint,
                                   const std::string& rawError) {
  if (rawError.empty()) {
    return {};
  }

  if (containsText(rawError, kMissingApiKeyNeedle)) {
    return std::string(isNoForeignLandType(endpoint.type) ? kMissingApiKeyNfl
                                                          : kMissingApiKeyJson);
  }

  for (const auto& pattern : kPatterns) {
    if (containsText(rawError, pattern.needle)) {
      return std::string(pattern.message);
    }
  }

  if (containsText(rawError, kGenericHttpNeedle)) {
    return std::string(kGenericHttpMessage);
  }

  return std::string(kFallbackMessage);
}

EndpointErrorUiState computeEndpointErrorUiState(
    const EndpointConfig* endpoint,
    const std::map<std::string, EndpointUiState>& statuses) {
  EndpointErrorUiState ui;
  if (endpoint == nullptr) {
    return ui;
  }

  const auto it = statuses.find(endpointStateKey(*endpoint));
  if (it == statuses.end() || it->second.lastErrorMessage.empty()) {
    return ui;
  }

  ui.visible = true;
  ui.details = it->second.lastErrorMessage;
  ui.summary = summarizeEndpointError(*endpoint, ui.details);
  // Only the timestamp is propagated here — the UI layer builds the
  // localised "Last call: <ts>" label so the prefix can be translated.
  ui.lastSentLocalTime = it->second.lastSentLocalTime;
  return ui;
}

}  // namespace tracker_pi
