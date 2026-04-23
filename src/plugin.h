#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>

#include <wx/bitmap.h>
#include <wx/string.h>

#include "ocpn_plugin.h"

#include "1tracker_pi/config_loader.h"
#include "1tracker_pi/endpoint_sender.h"
#include "1tracker_pi/endpoint_ui_state.h"
#include "1tracker_pi/payload_builder.h"
#include "1tracker_pi/scheduler.h"
#include "1tracker_pi/state_store.h"
#include "tracker_dialog.h"

class TrackerDialog;

class OneTrackerPi final : public opencpn_plugin_118 {
public:
  explicit OneTrackerPi(void* ppimgr);
  ~OneTrackerPi() override;

  int Init() override;
  bool DeInit() override;

  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetPlugInVersionPatch() override;

  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;
  int GetToolbarToolCount() override;
  void OnToolbarToolCallback(int id) override;
  void ShowPreferencesDialog(wxWindow* parent) override;

  void SetPositionFix(PlugIn_Position_Fix& pfix) override;
  void SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) override;
  void SetNMEASentence(wxString& sentence) override;

private:
  enum class DialogParentPolicy { UseAsIs, ResolveToTopLevel };
  enum class ToolbarState { Neutral, Green, Red };
  static constexpr int kPluginCapabilities =
      WANTS_NMEA_SENTENCES | WANTS_NMEA_EVENTS | WANTS_PREFERENCES |
      WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL;

  void openTrackerDialog(TrackerDialogEntryMode mode, wxWindow* parent,
                         DialogParentPolicy parentPolicy,
                         const char* sourceTag);
  std::map<std::string, tracker_pi::EndpointUiState> snapshotEndpointStatuses() const;
  void handleDialogOpenFailure(wxWindow* parent, const std::string& message);
  void handleWindSentence(const wxString& sentence);
  void createScheduler();
  void configureAndStartScheduler();
  void initializeToolbarTool();
  void logInitSummary() const;
  std::filesystem::path getPrivateDataRoot() const;
  std::filesystem::path getFallbackConfigPath() const;
  std::filesystem::path getConfigPath() const;
  void initializeConfigPath();
  void logConfigPathResolution() const;
  void createDefaultConfigIfMissing();
  bool loadConfigurationFromPath(const std::filesystem::path& path);
  bool loadFallbackConfiguration();
  bool writeRuntimeConfigFile(const tracker_pi::RuntimeConfig& config) const;
  void loadConfiguration();
  bool saveConfiguration(const tracker_pi::RuntimeConfig& config,
                         std::string* errorMessage = nullptr) const;
  void pruneEndpointStatuses();
  void syncSchedulerWithRuntimeConfig();
  void applyRuntimeConfig(const tracker_pi::RuntimeConfig& config);
  void updateEndpointStatus(const tracker_pi::EndpointConfig& endpoint,
                            const tracker_pi::EndpointSender::Result& result);
  ToolbarState computeToolbarState() const;
  void loadToolbarTemplate();
  wxBitmap renderToolbarBitmap(ToolbarState state) const;
  void refreshToolbarIcon();
  void logMessage(const std::string& message) const;

  tracker_pi::ConfigLoader configLoader_;
  tracker_pi::StateStore stateStore_;
  tracker_pi::PayloadBuilder payloadBuilder_;
  tracker_pi::EndpointSender endpointSender_;
  std::unique_ptr<tracker_pi::Scheduler> scheduler_;
  tracker_pi::RuntimeConfig runtimeConfig_;
  std::filesystem::path configPath_;
  // std::shared_mutex instead of std::mutex — see state_store.h for the
  // reason (MSVC/CRT mutex indirection crashes on some target machines).
  mutable std::shared_mutex endpointStatusMutex_;
  std::map<std::string, tracker_pi::EndpointUiState> endpointStatuses_;
  bool initialized_ = false;
  // Flips to true at the top of DeInit(). Any UI-bound work posted from other
  // threads must bail out if this is set, since wx may dispatch queued calls
  // after the plugin host has already destroyed us.
  std::atomic<bool> shutdownRequested_{false};
  int toolbarToolId_ = -1;
  std::string toolbarTemplate_;
  TrackerDialog* trackerDialog_ = nullptr;
};
