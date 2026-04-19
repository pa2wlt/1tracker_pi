#pragma once

#include <filesystem>
#include <functional>
#include <map>

#include "1tracker_pi/endpoint_ui_state.h"
#include "1tracker_pi/runtime_config.h"

class wxWindow;
class TrackerDialog;

enum class TrackerDialogEntryMode {
  Trackers,
  Preferences,
};

TrackerDialog* CreateTrackerDialog(
    wxWindow* parent, const std::filesystem::path& configPath,
    const tracker_pi::RuntimeConfig& initialConfig,
    const std::map<std::string, tracker_pi::EndpointUiState>& endpointStatuses,
    std::function<bool(const tracker_pi::RuntimeConfig&, std::string*)> applyConfigFn,
    TrackerDialogEntryMode entryMode = TrackerDialogEntryMode::Trackers,
    std::function<void()> onClose = nullptr);

void ShowTrackerDialog(TrackerDialog* dialog);
void RaiseTrackerDialog(TrackerDialog* dialog);
void FocusTrackerDialog(TrackerDialog* dialog);
void DestroyTrackerDialog(TrackerDialog* dialog);
