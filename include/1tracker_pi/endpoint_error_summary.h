#pragma once

#include <map>
#include <string>

#include "1tracker_pi/endpoint_config.h"
#include "1tracker_pi/endpoint_ui_state.h"

namespace tracker_pi {

std::string summarizeEndpointError(const EndpointConfig& endpoint,
                                   const std::string& rawError);

struct EndpointErrorUiState {
  bool visible = false;
  std::string summary;            // user-facing summary of the raw error
                                  // (marked TR_NOOP in the source; UI
                                  // passes through wxGetTranslation)
  std::string details;            // raw error text as reported by the
                                  // sender — not translated
  std::string lastSentLocalTime;  // timestamp only, empty if no recorded
                                  // send yet. UI composes the localised
                                  // "Last call: <ts>" label.
};

// Pure: decides what the error panel should show for a given endpoint.
// Returns visible=false when there is no endpoint, no recorded status, or no error.
EndpointErrorUiState computeEndpointErrorUiState(
    const EndpointConfig* endpoint,
    const std::map<std::string, EndpointUiState>& statuses);

}  // namespace tracker_pi
