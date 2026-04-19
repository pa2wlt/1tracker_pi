#include "plugin.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <json/json.h>

#include <wx/button.h>
#include <wx/datetime.h>
#include <wx/dialog.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/window.h>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/nfl_settings.h"
#include "1tracker_pi/plugin_metadata.h"
#include "1tracker_pi/version.h"
#include "plugin_ui_utils.h"
#include "tracker_dialog.h"

namespace {

constexpr int kApiVersionMajor = 1;
constexpr int kApiVersionMinor = 18;
constexpr int kPreferencesDialogWidth = 576;
const char* kToolbarBaseIcon = "1tracker_toolbar_icon.svg";
const char* kToolbarNeutralIcon = "toolbar_icon_neutral.svg";
const char* kToolbarGreenIcon = "toolbar_icon_green.svg";
const char* kToolbarRedIcon = "toolbar_icon_red.svg";
const char* kToolbarColorWhite = "#d0d0d0";
const char* kToolbarColorBlack = "#333333";
const char* kToolbarColorGreen = "#2e7d32";
const char* kToolbarColorRed = "#c62828";
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

struct ToolbarIconPaths {
  std::filesystem::path neutral;
  std::filesystem::path green;
  std::filesystem::path red;
};

std::string toolbarCacheBustToken(const std::filesystem::path& sourcePath) {
  try {
    const auto ticks = std::filesystem::last_write_time(sourcePath)
                           .time_since_epoch()
                           .count();
    return std::to_string(static_cast<long long>(ticks));
  } catch (...) {
    return "0";
  }
}

std::filesystem::path toolbarIconVariantPath(
    const std::filesystem::path& outputDir, const char* baseName,
    const std::string& cacheBustToken) {
  const std::filesystem::path basePath(baseName);
  return outputDir / (basePath.stem().string() + "_" + cacheBustToken +
                      basePath.extension().string());
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

ToolbarIconPaths buildToolbarIconPaths(const std::filesystem::path& outputDir,
                                       const std::filesystem::path& sourcePath) {
  const std::string cacheBustToken = toolbarCacheBustToken(sourcePath);
  return ToolbarIconPaths{
      toolbarIconVariantPath(outputDir, kToolbarNeutralIcon, cacheBustToken),
      toolbarIconVariantPath(outputDir, kToolbarGreenIcon, cacheBustToken),
      toolbarIconVariantPath(outputDir, kToolbarRedIcon, cacheBustToken)};
}

wxBitmap loadToolbarBitmap(const std::filesystem::path& iconPath) {
  if (!std::filesystem::exists(iconPath)) {
    return wxNullBitmap;
  }

  const wxString svgPath = wxString::FromUTF8(iconPath.string().c_str());
  return GetBitmapFromSVGFile(svgPath, kToolbarBitmapSize, kToolbarBitmapSize);
}

bool writeColoredToolbarIcon(const std::filesystem::path& sourcePath,
                             const std::filesystem::path& targetPath,
                             const ToolbarIconColors& colors) {
  std::ifstream input(sourcePath);
  if (!input.is_open()) {
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  std::string svg = replaceAll(buffer.str(), "__MARKER_FILL__", colors.markerFill);
  svg = replaceAll(svg, "__MARKER_STROKE__", colors.markerStroke);
  svg = replaceAll(svg, "__BOAT_FILL__", colors.boatFill);

  std::filesystem::create_directories(targetPath.parent_path());
  std::ofstream output(targetPath);
  if (!output.is_open()) {
    return false;
  }

  output << svg;
  return true;
}

}  // namespace

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new OneTrackerPi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

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
  loadConfiguration();
  scheduler_->configure(runtimeConfig_);

  if (runtimeConfig_.enabled) {
    scheduler_->start();
  } else {
    logMessage("1tracker_pi: plugin is disabled by configuration");
  }
}

void OneTrackerPi::initializeToolbarTool() {
  refreshToolbarIcon();
  const auto toolbarIconPaths = buildToolbarIconPaths(
      configPath_.parent_path(),
      tracker_plugin_ui::FindPluginAssetPath(kToolbarBaseIcon).ToStdString());
  const auto toolbarIconPath = toolbarIconPaths.neutral;
  const wxBitmap trackerBitmap = loadToolbarBitmap(toolbarIconPath);
  logMessage("1tracker_pi: toolbar icon=" + toolbarIconPath.string());

  if (trackerBitmap.IsOk()) {
    wxBitmap normalBitmap(trackerBitmap);
    wxBitmap rolloverBitmap(trackerBitmap);
    toolbarToolId_ = InsertPlugInTool(
        "1tracker", &normalBitmap, &rolloverBitmap, wxITEM_NORMAL, "1tracker",
        "Open tracker screen", nullptr, -1, 0, this);
  } else {
    toolbarToolId_ = InsertPlugInTool(
        "1tracker", GetPlugInBitmap(), GetPlugInBitmap(), wxITEM_NORMAL,
        "1tracker", "Open tracker screen", nullptr, -1, 0, this);
  }

  refreshToolbarIcon();
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
  logMessage("1tracker_pi: Init() enter");
  initialized_ = true;

  createScheduler();
  configureAndStartScheduler();
  initializeToolbarTool();
  logInitSummary();

  return kPluginCapabilities;
}

bool OneTrackerPi::DeInit() {
  if (!initialized_) {
    logMessage("1tracker_pi: DeInit() called before Init(), nothing to do");
    return true;
  }

  logMessage("1tracker_pi: DeInit() enter");

  if (trackerDialog_ != nullptr) {
    DestroyTrackerDialog(trackerDialog_);
    trackerDialog_ = nullptr;
  }

  if (scheduler_) {
    scheduler_->stop();
    scheduler_.reset();
  }
  if (toolbarToolId_ != -1) {
    RemovePlugInTool(toolbarToolId_);
    toolbarToolId_ = -1;
  }

  initialized_ = false;
  logMessage("1tracker_pi: DeInit() complete");
  return true;
}

int OneTrackerPi::GetAPIVersionMajor() { return kApiVersionMajor; }

int OneTrackerPi::GetAPIVersionMinor() { return kApiVersionMinor; }

int OneTrackerPi::GetPlugInVersionMajor() { return ONETRACKER_PLUGIN_VERSION_MAJOR; }

int OneTrackerPi::GetPlugInVersionMinor() { return ONETRACKER_PLUGIN_VERSION_MINOR; }

int OneTrackerPi::GetPlugInVersionPatch() { return ONETRACKER_PLUGIN_VERSION_PATCH; }

wxString OneTrackerPi::GetCommonName() { return "1tracker"; }

wxString OneTrackerPi::GetShortDescription() {
  return tracker_pi::kPluginSummary;
}

wxString OneTrackerPi::GetLongDescription() {
  return tracker_pi::kPluginDescription;
}

int OneTrackerPi::GetToolbarToolCount() { return 1; }

void OneTrackerPi::OnToolbarToolCallback(int id) {
  if (id == toolbarToolId_) {
    openTrackerDialog(TrackerDialogEntryMode::Trackers, GetOCPNCanvasWindow(),
                      DialogParentPolicy::ResolveToTopLevel, "toolbar");
  }
}

void OneTrackerPi::ShowPreferencesDialog(wxWindow* parent) {
  wxWindow* dialogParent = parent;
  if (dialogParent == nullptr) {
    dialogParent = GetActiveOptionsDialog();
  }
  if (dialogParent == nullptr) {
    dialogParent = GetOCPNCanvasWindow();
  }

  openTrackerDialog(TrackerDialogEntryMode::Preferences, dialogParent,
                    DialogParentPolicy::UseAsIs, "preferences");
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

  std::map<std::string, tracker_pi::EndpointUiState> endpointStatuses;
  {
    std::lock_guard<std::mutex> lock(endpointStatusMutex_);
    endpointStatuses = endpointStatuses_;
  }

  trackerDialog_ = CreateTrackerDialog(
      resolvedParent, configPath_, runtimeConfig_, endpointStatuses,
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
}

void OneTrackerPi::updateEndpointStatus(
    const tracker_pi::EndpointConfig& endpoint,
    const tracker_pi::EndpointSender::Result& result) {
  {
    std::lock_guard<std::mutex> lock(endpointStatusMutex_);
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
  refreshToolbarIcon();
}

void OneTrackerPi::SetPositionFix(PlugIn_Position_Fix& pfix) {
  stateStore_.updateLatLon(pfix.Lat, pfix.Lon);
  stateStore_.updateTimevalue(static_cast<std::int64_t>(pfix.FixTime));
}

void OneTrackerPi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  stateStore_.updateLatLon(pfix.Lat, pfix.Lon);
  stateStore_.updateTimevalue(static_cast<std::int64_t>(pfix.FixTime));
}

void OneTrackerPi::SetNMEASentence(wxString& sentence) { handleWindSentence(sentence); }

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
  initializeConfigPath();
  logConfigPathResolution();
  createDefaultConfigIfMissing();

  try {
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
  std::filesystem::create_directories(configPath_.parent_path());

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

  std::ofstream stream(configPath_);
  if (!stream.is_open()) {
    throw std::runtime_error("Unable to open config file for writing: " +
                             configPath_.string());
  }

  stream << Json::writeString(builder, root) << '\n';
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
    std::lock_guard<std::mutex> lock(endpointStatusMutex_);
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
  std::lock_guard<std::mutex> lock(endpointStatusMutex_);
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

void OneTrackerPi::ensureToolbarIconAssets(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputDir) const {
  const ToolbarIconPaths iconPaths = buildToolbarIconPaths(outputDir, sourcePath);
  writeColoredToolbarIcon(
      sourcePath, iconPaths.neutral,
      ToolbarIconColors{kToolbarColorBlack, kToolbarColorWhite,
                        kToolbarColorWhite});
  writeColoredToolbarIcon(
      sourcePath, iconPaths.green,
      ToolbarIconColors{kToolbarColorGreen, kToolbarColorWhite,
                        kToolbarColorWhite});
  writeColoredToolbarIcon(
      sourcePath, iconPaths.red,
      ToolbarIconColors{kToolbarColorRed, kToolbarColorWhite, kToolbarColorWhite});
}

void OneTrackerPi::applyToolbarIcon(const std::filesystem::path& iconPath) {
  if (toolbarToolId_ == -1 || !std::filesystem::exists(iconPath)) {
    return;
  }

  wxBitmap normalBitmap = loadToolbarBitmap(iconPath);
  if (!normalBitmap.IsOk()) {
    return;
  }

  wxBitmap rolloverBitmap(normalBitmap);
  SetToolbarToolBitmaps(toolbarToolId_, &normalBitmap, &rolloverBitmap);
}

void OneTrackerPi::refreshToolbarIcon() {
  const wxString baseIcon = tracker_plugin_ui::FindPluginAssetPath(kToolbarBaseIcon);
  if (baseIcon.empty() || configPath_.empty()) {
    return;
  }

  const std::filesystem::path sourcePath(baseIcon.ToStdString());
  const std::filesystem::path outputDir = configPath_.parent_path();
  const ToolbarIconPaths iconPaths = buildToolbarIconPaths(outputDir, sourcePath);
  ensureToolbarIconAssets(sourcePath, outputDir);

  std::filesystem::path iconPath = iconPaths.neutral;
  const ToolbarState toolbarState = computeToolbarState();
  if (toolbarState == ToolbarState::Green) {
    iconPath = iconPaths.green;
  } else if (toolbarState == ToolbarState::Red) {
    iconPath = iconPaths.red;
  }

  applyToolbarIcon(iconPath);
}

void OneTrackerPi::logMessage(const std::string& message) const {
  wxLogMessage("%s", message);
}
