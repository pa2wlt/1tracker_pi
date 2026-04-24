#pragma once

#include <optional>
#include <string>

class wxWindow;

namespace tracker_plugin_ui {

// Presents a modal "pick a tracker type" dialog. Returns the chosen type
// string (matching one of tracker_pi::kEndpointType* constants) on OK, or
// std::nullopt if the user cancelled. NFL is pre-selected to match the
// most common new-tracker case.
std::optional<std::string> PickEndpointType(wxWindow* parent);

}  // namespace tracker_plugin_ui
