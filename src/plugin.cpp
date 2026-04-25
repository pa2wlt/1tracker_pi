#include "plugin.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <json/json.h>

#include <wx/app.h>
#if wxCHECK_VERSION(3, 1, 6)
#include <wx/bmpbndl.h>
#endif
#include <wx/button.h>
#include <wx/datetime.h>
#include <wx/dialog.h>
#include <wx/file.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/window.h>

#include "1tracker_pi/atomic_file_writer.h"
#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/nfl_settings.h"
#include "1tracker_pi/plugin_metadata.h"
#include "1tracker_pi/version.h"
#include "crash_guard.h"
#include "plugin_ui_utils.h"
#include "tracker_dialog.h"

namespace {

constexpr int kApiVersionMajor = 1;
constexpr int kApiVersionMinor = 18;
constexpr int kPreferencesDialogWidth = 576;
const char* kToolbarBaseIcon = "1tracker_toolbar_icon.svg";
const char* kToolbarColorWhite = "#d0d0d0";
const char* kToolbarColorBlack = "#333333";
const char* kToolbarColorGreen = "#2e7d32";
const char* kToolbarColorRed = "#c62828";
#ifdef __ANDROID__
// Pre-rendered PNG fallbacks used on Android, where the wxQt build can't
// rasterize SVG via wxBitmapBundle::FromSVG (the toolbar would silently
// stay invisible). Three states; mirror the SVG's color cycling.
const char* kToolbarPngNeutral = "1tracker_toolbar_icon_neutral.png";
const char* kToolbarPngGreen = "1tracker_toolbar_icon_green.png";
const char* kToolbarPngRed = "1tracker_toolbar_icon_red.png";
#endif
constexpr unsigned int kToolbarBitmapSize = 32;

std::vector<std::string> split(const std::string& value, char separator) {
  std::vector<std::string> parts;
  std::string current;
  for (const char ch : value) {
    if (ch == separator) {
      parts.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  parts.push_back(current);
  return parts;
}

std::string replaceAll(std::string text, const std::string& from,
                       const std::string& to) {
  std::size_t position = 0;
  while ((position = text.find(from, position)) != std::string::npos) {
    text.replace(position, from.size(), to);
    position += to.size();
  }
  return text;
}

double windSpeedToKnots(double value, const std::string& units) {
  if (units == "N") {
    return value;
  }
  if (units == "K") {
    return value * 0.539956803;
  }
  if (units == "M") {
    return value * 1.94384449;
  }
  return value;
}

std::string formatLocalTimestampNow() {
  return wxDateTime::Now().Format("%d-%m-%Y %H:%M:%S").ToStdString();
}

tracker_pi::RuntimeConfig makeDefaultRuntimeConfig() {
  tracker_pi::RuntimeConfig config;
  config.enabled = true;
  return config;
}

struct ToolbarIconColors {
  std::string markerFill;
  std::string markerStroke;
  std::string boatFill;
};

wxBitmap renderSvgStringToBitmap(const std::string& svg,
                                 int pxSize = kToolbarBitmapSize) {
#if wxCHECK_VERSION(3, 1, 6)
  if (svg.empty()) {
    return wxNullBitmap;
  }
  const wxSize size(pxSize, pxSize);
  return wxBitmapBundle::FromSVG(svg.c_str(), size).GetBitmap(size);
#else
  (void)svg;
  (void)pxSize;
  return wxNullBitmap;
#endif
}

wxWindow* resolveDialogParent(wxWindow* candidate) {
  if (candidate == nullptr) {
    return nullptr;
  }

  wxWindow* topLevelParent = wxGetTopLevelParent(candidate);
  return topLevelParent != nullptr ? topLevelParent : candidate;
}

std::string describeWindow(wxWindow* window) {
  if (window == nullptr) {
    return "null";
  }

  std::ostringstream stream;
  stream << window;
  const wxString name = window->GetName();
  const wxString label = window->GetLabel();
  stream << " name=\"" << std::string(name.ToUTF8()) << "\"";
  if (!label.empty()) {
    stream << " label=\"" << std::string(label.ToUTF8()) << "\"";
  }
  stream << " shown=" << (window->IsShown() ? "true" : "false");
  return stream.str();
}

std::string applyToolbarColors(const std::string& templateSvg,
                               const ToolbarIconColors& colors) {
  std::string svg = replaceAll(templateSvg, "__MARKER_FILL__", colors.markerFill);
  svg = replaceAll(svg, "__MARKER_STROKE__", colors.markerStroke);
  svg = replaceAll(svg, "__BOAT_FILL__", colors.boatFill);
  return svg;
}

}  // namespace

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  // This is the first entry point OCPN calls on the plugin's DLL. Install
  // the SEH translator here too — so even a fault during `new OneTrackerPi`
  // itself turns into a catchable C++ exception rather than a process
  // crash.
  tracker_pi::installSehTranslator();
  try {
    return new OneTrackerPi(ppimgr);
  } catch (const std::exception& error) {
    wxLogMessage("1tracker_pi: create_pi caught exception: %s", error.what());
    return nullptr;
  } catch (...) {
    wxLogMessage("1tracker_pi: create_pi caught unknown exception");
    return nullptr;
  }
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
  tracker_pi::installSehTranslator();
  try {
    delete p;
  } catch (const std::exception& error) {
    wxLogMessage("1tracker_pi: destroy_pi caught exception: %s", error.what());
  } catch (...) {
    wxLogMessage("1tracker_pi: destroy_pi caught unknown exception");
  }
}

OneTrackerPi::OneTrackerPi(void* ppimgr) : opencpn_plugin_118(ppimgr) {}

OneTrackerPi::~OneTrackerPi() {
  if (initialized_) {
    DeInit();
  }
}

void OneTrackerPi::createScheduler() {
  scheduler_ = std::make_unique<tracker_pi::Scheduler>(
      stateStore_, payloadBuilder_, endpointSender_,
      [this](const std::string& message) { logMessage(message); },
      [this](const tracker_pi::EndpointConfig& endpoint,
             const tracker_pi::EndpointSender::Result& result) {
        updateEndpointStatus(endpoint, result);
      });
}

void OneTrackerPi::configureAndStartScheduler() {
  logMessage("1tracker_pi: configureAndStartScheduler step=loadConfiguration");
  loadConfiguration();

  if (!scheduler_) {
    logMessage("1tracker_pi: scheduler unavailable; skipping configure/start");
    return;
  }

  logMessage("1tracker_pi: configureAndStartScheduler step=scheduler.configure");
  scheduler_->configure(runtimeConfig_);

  logMessage("1tracker_pi: configureAndStartScheduler step=check_enabled enabled=" +
             std::string(runtimeConfig_.enabled ? "true" : "false"));
  if (runtimeConfig_.enabled) {
    logMessage("1tracker_pi: configureAndStartScheduler step=scheduler.start");
    scheduler_->start();
    logMessage("1tracker_pi: configureAndStartScheduler step=scheduler.start_done");
  } else {
    logMessage("1tracker_pi: plugin is disabled by configuration");
  }
  logMessage("1tracker_pi: configureAndStartScheduler step=done");
}

void OneTrackerPi::initializeToolbarTool() {
  logMessage("1tracker_pi: toolbar init step=load_template");
  loadToolbarTemplate();

  logMessage("1tracker_pi: toolbar init step=render_neutral");
  wxBitmap neutralBitmap;
  try {
    neutralBitmap = renderToolbarBitmap(ToolbarState::Neutral);
  } catch (const std::exception& error) {
    logMessage(std::string("1tracker_pi: renderToolbarBitmap threw: ") +
               error.what());
  } catch (...) {
    logMessage("1tracker_pi: renderToolbarBitmap threw unknown exception");
  }

  logMessage(std::string("1tracker_pi: toolbar init step=insert_tool, "
                         "neutralBitmap_ok=") +
             (neutralBitmap.IsOk() ? "true" : "false"));
  try {
    if (neutralBitmap.IsOk()) {
      wxBitmap normalBitmap(neutralBitmap);
      wxBitmap rolloverBitmap(neutralBitmap);
      toolbarToolId_ = InsertPlugInTool(
          "1tracker", &normalBitmap, &rolloverBitmap, wxITEM_NORMAL, "1tracker",
          "Open tracker screen", nullptr, -1, 0, this);
    } else {
      // Defensive fallback: if our SVG didn't render, don't blindly hand
      // OCPN whatever GetPlugInBitmap() returns — a null bitmap here has
      // historically crashed OpenCPN on some Windows builds. Skip the tool
      // entirely rather than risk a bad bitmap reaching the toolbar code.
      toolbarToolId_ = -1;
      logMessage("1tracker_pi: neutral bitmap invalid; skipping toolbar tool "
                 "registration to avoid a crash");
    }
  } catch (const std::exception& error) {
    toolbarToolId_ = -1;
    logMessage(std::string("1tracker_pi: InsertPlugInTool threw: ") +
               error.what());
  } catch (...) {
    toolbarToolId_ = -1;
    logMessage("1tracker_pi: InsertPlugInTool threw unknown exception");
  }

  logMessage("1tracker_pi: toolbar tool id=" + std::to_string(toolbarToolId_) +
             ", template_bytes=" + std::to_string(toolbarTemplate_.size()));
  if (toolbarToolId_ == -1) {
    logMessage("1tracker_pi: WARNING toolbar button will not be visible");
    return;
  }

  logMessage("1tracker_pi: toolbar init step=refresh_icon");
  try {
    refreshToolbarIcon();
  } catch (const std::exception& error) {
    logMessage(std::string("1tracker_pi: refreshToolbarIcon threw: ") +
               error.what());
  } catch (...) {
    logMessage("1tracker_pi: refreshToolbarIcon threw unknown exception");
  }
  logMessage("1tracker_pi: toolbar init step=done");
}

void OneTrackerPi::logInitSummary() const {
  std::ostringstream stream;
  stream << "1tracker_pi: Init() complete, enabled="
         << (runtimeConfig_.enabled ? "true" : "false")
         << ", endpoints=" << runtimeConfig_.endpoints.size()
         << ", toolbarToolId=" << toolbarToolId_
         << ", capabilities=" << kPluginCapabilities;
  logMessage(stream.str());
}

int OneTrackerPi::Init() {
  // Install the Windows SEH translator before we log anything: if *this*
  // function blows up with an access violation, the translator turns it into
  // a catchable std::runtime_error instead of a silent crash. No-op off-Win.
  tracker_pi::installSehTranslator();

  logMessage("1tracker_pi: Init() enter");
  initialized_ = true;

  // Install the process-wide unhandled-exception filter so any future crash
  // writes a minidump next to the config. Uses the private data root if
  // available; falls back to the current dir. Windows-only in practice.
  try {
    const auto dumpDir = getPrivateDataRoot().empty()
                             ? std::filesystem::path("crash-dumps")
                             : getPrivateDataRoot() / "plugins" /
                                   "1tracker_pi" / "crash-dumps";
    tracker_pi::installCrashHandler(dumpDir);
  } catch (...) {
    // Installing the crash handler must never itself crash Init().
  }

  auto logger = [this](const std::string& message) { logMessage(message); };

  tracker_pi::runGuarded("Init:createScheduler",
                         [this] { createScheduler(); }, logger);
  tracker_pi::runGuarded("Init:configureAndStartScheduler",
                         [this] { configureAndStartScheduler(); }, logger);
  tracker_pi::runGuarded("Init:initializeToolbarTool",
                         [this] { initializeToolbarTool(); }, logger);
  tracker_pi::runGuarded("Init:logInitSummary",
                         [this] { logInitSummary(); }, logger);

  return kPluginCapabilities;
}

bool OneTrackerPi::DeInit() {
  tracker_pi::installSehTranslator();

  if (!initialized_) {
    logMessage("1tracker_pi: DeInit() called before Init(), nothing to do");
    return true;
  }

  logMessage("1tracker_pi: DeInit() enter");

  // Flip the shutdown flag BEFORE stopping the scheduler so any CallAfter
  // posted by an in-flight send sees it and no-ops instead of touching a
  // toolbar that's about to be removed.
  shutdownRequested_ = true;

  auto logger = [this](const std::string& message) { logMessage(message); };

  tracker_pi::runGuarded("DeInit:destroyDialog", [this] {
    if (trackerDialog_ != nullptr) {
      DestroyTrackerDialog(trackerDialog_);
      trackerDialog_ = nullptr;
    }
  }, logger);

  tracker_pi::runGuarded("DeInit:stopScheduler", [this] {
    if (scheduler_) {
      scheduler_->stop();
      scheduler_.reset();
    }
  }, logger);

  tracker_pi::runGuarded("DeInit:removeToolbarTool", [this] {
    if (toolbarToolId_ != -1) {
      RemovePlugInTool(toolbarToolId_);
      toolbarToolId_ = -1;
    }
  }, logger);

  initialized_ = false;
  logMessage("1tracker_pi: DeInit() complete");
  return true;
}

int OneTrackerPi::GetAPIVersionMajor() { return kApiVersionMajor; }

int OneTrackerPi::GetAPIVersionMinor() { return kApiVersionMinor; }

int OneTrackerPi::GetPlugInVersionMajor() { return ONETRACKER_PLUGIN_VERSION_MAJOR; }

int OneTrackerPi::GetPlugInVersionMinor() { return ONETRACKER_PLUGIN_VERSION_MINOR; }

int OneTrackerPi::GetPlugInVersionPatch() { return ONETRACKER_PLUGIN_VERSION_PATCH; }

wxString OneTrackerPi::GetCommonName() { return tracker_pi::kPluginCommonName; }

wxString OneTrackerPi::GetShortDescription() {
  return tracker_pi::kPluginSummary;
}

wxString OneTrackerPi::GetLongDescription() {
  return tracker_pi::kPluginDescription;
}

// Icon shown next to the plugin entry in OpenCPN's Options → Plugins list.
// We reuse the toolbar SVG template with the "green marker" colour set, so
// the list icon matches the brand colour shown when the plugin is happily
// sending. The bitmap must stay alive for the duration OpenCPN holds the
// pointer, so it's a function-local static.
wxBitmap* OneTrackerPi::GetPlugInBitmap() {
  static wxBitmap bitmap;
  if (!bitmap.IsOk()) {
    // OpenCPN may call GetPlugInBitmap() before Init() loads the toolbar
    // template. Lazy-load here so the green icon is available on first
    // render regardless of call order.
    if (toolbarTemplate_.empty()) {
      loadToolbarTemplate();
    }
    bitmap = renderToolbarBitmap(ToolbarState::Green, 128);
    if (!bitmap.IsOk()) {
      // Fallback for older wx where the SVG renderer is unavailable: use
      // the neutral asset. Still better than the empty-square default.
      bitmap = tracker_plugin_ui::LoadBitmapFromPluginAsset(
          "1tracker_icon.svg", wxSize(128, 128));
    }
  }
  return &bitmap;
}

int OneTrackerPi::GetToolbarToolCount() { return 1; }

void OneTrackerPi::OnToolbarToolCallback(int id) {
  tracker_pi::installSehTranslator();
  tracker_pi::runGuarded("OnToolbarToolCallback", [this, id] {
    if (id == toolbarToolId_) {
      openTrackerDialog(TrackerDialogEntryMode::Trackers, GetOCPNCanvasWindow(),
                        DialogParentPolicy::ResolveToTopLevel, "toolbar");
    }
  }, [this](const std::string& m) { logMessage(m); });
}

void OneTrackerPi::ShowPreferencesDialog(wxWindow* parent) {
  tracker_pi::installSehTranslator();
  tracker_pi::runGuarded("ShowPreferencesDialog", [this, parent] {
    wxWindow* dialogParent = parent;
    if (dialogParent == nullptr) {
      dialogParent = GetActiveOptionsDialog();
    }
    if (dialogParent == nullptr) {
      dialogParent = GetOCPNCanvasWindow();
    }

    openTrackerDialog(TrackerDialogEntryMode::Preferences, dialogParent,
                      DialogParentPolicy::UseAsIs, "preferences");
  }, [this](const std::string& m) { logMessage(m); });
}

std::map<std::string, tracker_pi::EndpointUiState>
OneTrackerPi::snapshotEndpointStatuses() const {
  std::lock_guard<std::shared_mutex> lock(endpointStatusMutex_);
  return endpointStatuses_;
}

void OneTrackerPi::handleDialogOpenFailure(wxWindow* parent,
                                            const std::string& message) {
  if (trackerDialog_ != nullptr) {
    DestroyTrackerDialog(trackerDialog_);
    trackerDialog_ = nullptr;
  }
  logMessage("1tracker_pi: failed to open tracker dialog: " + message);
  wxMessageBox(wxString::FromUTF8(message.c_str()), "1tracker",
               wxOK | wxICON_ERROR, parent);
}

void OneTrackerPi::openTrackerDialog(TrackerDialogEntryMode mode,
                                     wxWindow* parent,
                                     DialogParentPolicy parentPolicy,
                                     const char* sourceTag) {
  wxWindow* resolvedParent =
      parentPolicy == DialogParentPolicy::ResolveToTopLevel
          ? resolveDialogParent(parent)
          : parent;
  wxWindow* topLevelParent = resolveDialogParent(parent);

  {
    std::ostringstream stream;
    stream << "1tracker_pi: openTrackerDialog source=" << sourceTag
           << ", mode="
           << (mode == TrackerDialogEntryMode::Preferences ? "preferences"
                                                           : "trackers")
           << ", parent=" << describeWindow(parent)
           << ", resolvedParent=" << describeWindow(resolvedParent)
           << ", topLevelParent=" << describeWindow(topLevelParent);
    logMessage(stream.str());
  }

  if (trackerDialog_ != nullptr) {
    RaiseTrackerDialog(trackerDialog_);
    FocusTrackerDialog(trackerDialog_);
    return;
  }

  try {
    trackerDialog_ = CreateTrackerDialog(
        resolvedParent, configPath_, runtimeConfig_, snapshotEndpointStatuses(),
        [this](const tracker_pi::RuntimeConfig& updatedConfig,
               std::string* errorMessage) {
          if (!saveConfiguration(updatedConfig, errorMessage)) {
            return false;
          }
          applyRuntimeConfig(updatedConfig);
          return true;
        },
        mode, [this] { trackerDialog_ = nullptr; });

    ShowTrackerDialog(trackerDialog_);
  } catch (const std::exception& error) {
    handleDialogOpenFailure(topLevelParent, error.what());
  } catch (...) {
    handleDialogOpenFailure(topLevelParent, "Unknown error opening tracker dialog");
  }
}

void OneTrackerPi::updateEndpointStatus(
    const tracker_pi::EndpointConfig& endpoint,
    const tracker_pi::EndpointSender::Result& result) {
  {
    std::lock_guard<std::shared_mutex> lock(endpointStatusMutex_);
    auto& endpointState =
        endpointStatuses_[tracker_pi::endpointStateKey(endpoint)];
    endpointState.status = result.success ? tracker_pi::EndpointUiStatus::Success
                                          : tracker_pi::EndpointUiStatus::Failure;
    endpointState.lastSentLocalTime = formatLocalTimestampNow();
    if (result.success) {
      endpointState.lastErrorMessage.clear();
    } else {
      endpointState.lastErrorMessage = result.message;
    }
  }
  // This runs on the scheduler's worker thread. wx UI calls (SetToolbarToolBitmaps)
  // must be marshalled to the main thread.
  if (wxTheApp != nullptr) {
    wxTheApp->CallAfter([this] {
      if (shutdownRequested_.load()) {
        return;
      }
      refreshToolbarIcon();
    });
  }
}

void OneTrackerPi::SetPositionFix(PlugIn_Position_Fix& pfix) {
  tracker_pi::installSehTranslator();
  // Split fix-struct read from stateStore write. If pfix itself is a
  // dangling reference, the AV fires on the read line; if it's the mutex
  // in updateLatLon, it fires on the write line. Distinguishes the two.
  tracker_pi::runGuarded("SetPositionFix:readFix", [&pfix] {
    volatile double lat = pfix.Lat;
    volatile double lon = pfix.Lon;
    volatile std::int64_t fixTime = static_cast<std::int64_t>(pfix.FixTime);
    (void)lat; (void)lon; (void)fixTime;
  }, [this](const std::string& m) { logMessage(m); });
  tracker_pi::runGuarded("SetPositionFix:updateState", [this, &pfix] {
    stateStore_.updateLatLon(pfix.Lat, pfix.Lon);
    stateStore_.updateTimevalue(static_cast<std::int64_t>(pfix.FixTime));
  }, [this](const std::string& m) { logMessage(m); });
}

void OneTrackerPi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  tracker_pi::installSehTranslator();
  tracker_pi::runGuarded("SetPositionFixEx:readFix", [&pfix] {
    volatile double lat = pfix.Lat;
    volatile double lon = pfix.Lon;
    volatile std::int64_t fixTime = static_cast<std::int64_t>(pfix.FixTime);
    (void)lat; (void)lon; (void)fixTime;
  }, [this](const std::string& m) { logMessage(m); });
  tracker_pi::runGuarded("SetPositionFixEx:updateState", [this, &pfix] {
    stateStore_.updateLatLon(pfix.Lat, pfix.Lon);
    stateStore_.updateTimevalue(static_cast<std::int64_t>(pfix.FixTime));
  }, [this](const std::string& m) { logMessage(m); });
}

void OneTrackerPi::SetNMEASentence(wxString& sentence) {
  tracker_pi::installSehTranslator();
  tracker_pi::runGuarded("SetNMEASentence", [this, &sentence] {
    handleWindSentence(sentence);
  }, [this](const std::string& m) { logMessage(m); });
}

void OneTrackerPi::handleWindSentence(const wxString& sentence) {
  const std::string raw = sentence.ToStdString();
  if (raw.find("MWV") == std::string::npos) {
    return;
  }

  const std::vector<std::string> parts = split(raw, ',');
  if (parts.size() < 6 || parts[5].empty() || parts[5][0] != 'A') {
    return;
  }

  try {
    stateStore_.updateAWA(std::stod(parts[1]));
    stateStore_.updateAWS(windSpeedToKnots(std::stod(parts[3]), parts[4]));
  } catch (...) {
    logMessage("1tracker_pi: ignoring invalid MWV sentence");
  }
}

std::filesystem::path OneTrackerPi::getConfigPath() const {
  const auto privateDataRoot = getPrivateDataRoot();
  if (!privateDataRoot.empty()) {
    return privateDataRoot / "plugins" / "1tracker_pi" / "config.json";
  }

  return getFallbackConfigPath();
}

std::filesystem::path OneTrackerPi::getPrivateDataRoot() const {
  const wxString* privateData = GetpPrivateApplicationDataLocation();
  if (privateData == nullptr || privateData->empty()) {
    return {};
  }

  return std::filesystem::path(privateData->ToStdString());
}

std::filesystem::path OneTrackerPi::getFallbackConfigPath() const {
  return std::filesystem::path("config") / "config.json";
}

void OneTrackerPi::initializeConfigPath() {
  configPath_ = getConfigPath();
  if (!configPath_.empty()) {
    configPath_ = std::filesystem::absolute(configPath_);
  }
}

void OneTrackerPi::logConfigPathResolution() const {
  const auto privateDataRoot = getPrivateDataRoot();
  std::ostringstream stream;
  if (!privateDataRoot.empty()) {
    stream << "1tracker_pi: private app data dir=" << privateDataRoot.string()
           << ", config path=" << configPath_.string();
  } else {
    stream << "1tracker_pi: no private app data dir reported, config path="
           << configPath_.string();
  }
  logMessage(stream.str());
}

void OneTrackerPi::createDefaultConfigIfMissing() {
  if (configPath_.empty() || std::filesystem::exists(configPath_)) {
    return;
  }

  runtimeConfig_ = makeDefaultRuntimeConfig();
  std::string errorMessage;
  if (saveConfiguration(runtimeConfig_, &errorMessage)) {
    logMessage("1tracker_pi: created default config at " + configPath_.string());
  } else {
    logMessage("1tracker_pi: failed to create default config at " +
               configPath_.string() + ": " + errorMessage);
  }
}

bool OneTrackerPi::loadConfigurationFromPath(const std::filesystem::path& path) {
  runtimeConfig_ = configLoader_.loadFromFile(path);
  tracker_pi::normalizeRuntimeConfig(runtimeConfig_);

  std::ostringstream stream;
  stream << "1tracker_pi: loaded config from " << path.string() << " with "
         << runtimeConfig_.endpoints.size() << " endpoint(s)";
  logMessage(stream.str());
  return true;
}

bool OneTrackerPi::loadFallbackConfiguration() {
  if (!getPrivateDataRoot().empty()) {
    return false;
  }

  try {
    configPath_ = std::filesystem::path("config") / "1tracker.example.json";
    logMessage("1tracker_pi: trying development fallback config at " +
               std::filesystem::absolute(configPath_).string());
    loadConfigurationFromPath(configPath_);
    configPath_ = std::filesystem::absolute(configPath_);
    return true;
  } catch (...) {
    return false;
  }
}

void OneTrackerPi::loadConfiguration() {
  runtimeConfig_ = tracker_pi::RuntimeConfig{};

  try {
    initializeConfigPath();
    logConfigPathResolution();
    createDefaultConfigIfMissing();
    loadConfigurationFromPath(configPath_);
  } catch (const std::exception& error) {
    if (loadFallbackConfiguration()) {
      return;
    }

    runtimeConfig_.enabled = false;
    logMessage(std::string("1tracker_pi: config load failed: ") + error.what());
  }
}

bool OneTrackerPi::writeRuntimeConfigFile(
    const tracker_pi::RuntimeConfig& config) const {
  Json::Value root(Json::objectValue);
  root["enabled"] = config.enabled;

  Json::Value endpoints(Json::arrayValue);
  for (const auto& endpoint : config.endpoints) {
    Json::Value value(Json::objectValue);
    value["id"] = endpoint.id;
    value["name"] = endpoint.name;
    value["type"] = endpoint.type;
    value["enabled"] = endpoint.enabled;
    value["includeAwaAws"] = endpoint.includeAwaAws;
    value["sendIntervalMinutes"] = endpoint.sendIntervalMinutes;
    value["minDistanceMeters"] = endpoint.minDistanceMeters;
    value["url"] = endpoint.url;
    value["timeoutSeconds"] = endpoint.timeoutSeconds;
    value["headerName"] = endpoint.headerName;
    value["headerValue"] = endpoint.headerValue;
    endpoints.append(value);
  }
  root["endpoints"] = endpoints;

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "  ";

  tracker_pi::writeFileAtomically(configPath_,
                                  Json::writeString(builder, root) + "\n");
  return true;
}

bool OneTrackerPi::saveConfiguration(const tracker_pi::RuntimeConfig& config,
                                     std::string* errorMessage) const {
  try {
    auto normalizedConfig = config;
    tracker_pi::normalizeRuntimeConfig(normalizedConfig);
    if (!tracker_pi::validateRuntimeConfig(normalizedConfig, errorMessage)) {
      return false;
    }
    return writeRuntimeConfigFile(normalizedConfig);
  } catch (const std::exception& error) {
    if (errorMessage != nullptr) {
      *errorMessage = error.what();
    }
    return false;
  }
}

void OneTrackerPi::pruneEndpointStatuses() {
  {
    std::lock_guard<std::shared_mutex> lock(endpointStatusMutex_);
    for (auto it = endpointStatuses_.begin(); it != endpointStatuses_.end();) {
      const bool exists = std::any_of(
          runtimeConfig_.endpoints.begin(), runtimeConfig_.endpoints.end(),
          [&](const tracker_pi::EndpointConfig& endpoint) {
            return tracker_pi::endpointStateKey(endpoint) == it->first;
          });
      if (!exists) {
        it = endpointStatuses_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void OneTrackerPi::syncSchedulerWithRuntimeConfig() {
  if (!scheduler_) {
    return;
  }

  scheduler_->configure(runtimeConfig_);
  if (runtimeConfig_.enabled) {
    if (!scheduler_->isRunning()) {
      scheduler_->start();
    }
  } else if (scheduler_->isRunning()) {
    scheduler_->stop();
  }
}

void OneTrackerPi::applyRuntimeConfig(const tracker_pi::RuntimeConfig& config) {
  runtimeConfig_ = config;
  tracker_pi::normalizeRuntimeConfig(runtimeConfig_);

  pruneEndpointStatuses();
  syncSchedulerWithRuntimeConfig();

  refreshToolbarIcon();
}

OneTrackerPi::ToolbarState OneTrackerPi::computeToolbarState() const {
  std::lock_guard<std::shared_mutex> lock(endpointStatusMutex_);
  bool hasEnabledEndpoint = false;
  bool allEnabledSuccessful = true;

  for (const auto& endpoint : runtimeConfig_.endpoints) {
    if (!endpoint.enabled) {
      continue;
    }

    hasEnabledEndpoint = true;
    const auto statusIt =
        endpointStatuses_.find(tracker_pi::endpointStateKey(endpoint));
    const auto status = statusIt != endpointStatuses_.end()
                            ? statusIt->second.status
                            : tracker_pi::EndpointUiStatus::NoStats;

    if (status == tracker_pi::EndpointUiStatus::Failure) {
      return ToolbarState::Red;
    }

    if (status != tracker_pi::EndpointUiStatus::Success) {
      allEnabledSuccessful = false;
    }
  }

  return hasEnabledEndpoint && allEnabledSuccessful ? ToolbarState::Green
                                                    : ToolbarState::Neutral;
}

void OneTrackerPi::loadToolbarTemplate() {
  toolbarTemplate_.clear();

  logMessage("1tracker_pi: loadToolbarTemplate step=resolve_asset_path");
  wxString baseIcon;
  try {
    baseIcon = tracker_plugin_ui::FindPluginAssetPath(kToolbarBaseIcon);
  } catch (const std::exception& error) {
    logMessage(std::string("1tracker_pi: FindPluginAssetPath threw: ") +
               error.what());
    return;
  } catch (...) {
    logMessage("1tracker_pi: FindPluginAssetPath threw unknown exception");
    return;
  }
  if (baseIcon.empty()) {
    logMessage("1tracker_pi: toolbar template not found for " +
               std::string(kToolbarBaseIcon));
    return;
  }
  logMessage("1tracker_pi: loadToolbarTemplate resolved path=" +
             std::string(baseIcon.ToUTF8()));

  wxFile file;
  if (!file.Open(baseIcon)) {
    logMessage("1tracker_pi: failed to open toolbar template at " +
               std::string(baseIcon.ToUTF8()));
    return;
  }

  wxString contents;
  if (!file.ReadAll(&contents)) {
    logMessage("1tracker_pi: failed to read toolbar template at " +
               std::string(baseIcon.ToUTF8()));
    return;
  }

  toolbarTemplate_ = std::string(contents.ToUTF8());
  logMessage("1tracker_pi: loaded toolbar template from " +
             std::string(baseIcon.ToUTF8()) + " (" +
             std::to_string(toolbarTemplate_.size()) + " bytes)");
}

wxBitmap OneTrackerPi::renderToolbarBitmap(ToolbarState state,
                                           int pxSize) const {
#ifdef __ANDROID__
  // wxQt on Android lacks the NanoSVG-based wxBitmapBundle::FromSVG that
  // the desktop builds rely on, so SVG-with-color-substitution silently
  // returns an invalid bitmap and the toolbar button never appears. Fall
  // back to three pre-rendered PNGs that mirror the SVG's color cycling.
  const char* fileName = kToolbarPngNeutral;
  if (state == ToolbarState::Green) {
    fileName = kToolbarPngGreen;
  } else if (state == ToolbarState::Red) {
    fileName = kToolbarPngRed;
  }
  return tracker_plugin_ui::LoadBitmapFromPluginAssetWidth(fileName, pxSize);
#else
  if (toolbarTemplate_.empty()) {
    return wxNullBitmap;
  }
  ToolbarIconColors colors{kToolbarColorBlack, kToolbarColorWhite,
                           kToolbarColorWhite};
  if (state == ToolbarState::Green) {
    colors.markerFill = kToolbarColorGreen;
  } else if (state == ToolbarState::Red) {
    colors.markerFill = kToolbarColorRed;
  }
  const std::string svg = applyToolbarColors(toolbarTemplate_, colors);
  return renderSvgStringToBitmap(svg, pxSize);
#endif
}

void OneTrackerPi::refreshToolbarIcon() {
  if (toolbarToolId_ == -1) {
    return;
  }
  wxBitmap bitmap = renderToolbarBitmap(computeToolbarState());
  if (!bitmap.IsOk()) {
    return;
  }
  wxBitmap rolloverBitmap(bitmap);
  SetToolbarToolBitmaps(toolbarToolId_, &bitmap, &rolloverBitmap);
}

void OneTrackerPi::logMessage(const std::string& message) const {
  wxLogMessage("%s", message);
}
