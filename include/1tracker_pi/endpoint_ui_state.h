#pragma once

#include <string>

namespace tracker_pi {

enum class EndpointUiStatus {
  Disabled,
  NoStats,
  Failure,
  Success,
};

struct EndpointUiState {
  EndpointUiStatus status = EndpointUiStatus::NoStats;
  std::string lastSentLocalTime;
  std::string lastErrorMessage;
};

}  // namespace tracker_pi
