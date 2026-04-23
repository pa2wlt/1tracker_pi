#include "1tracker_pi/scheduler.h"
#include "1tracker_pi/endpoint_policy.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>  // for EXCEPTION_POINTERS — must come before <eh.h>
#include <eh.h>
#include <cstdio>
#include <stdexcept>
#endif

namespace tracker_pi {
namespace {

std::string endpointScheduleKey(std::size_t index, const EndpointConfig& endpoint) {
  if (!endpoint.id.empty()) {
    return endpoint.id;
  }
  return std::to_string(index) + ":" + endpoint.name;
}

std::string formatWallClock(std::chrono::system_clock::time_point timePoint) {
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(timePoint);
  std::tm localTime{};
#if defined(_WIN32)
  localtime_s(&localTime, &nowTime);
#else
  localtime_r(&nowTime, &localTime);
#endif

  std::ostringstream stream;
  stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

std::string formatWallClockNow() {
  return formatWallClock(std::chrono::system_clock::now());
}

long long durationMillis(std::chrono::steady_clock::duration duration) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

double toRadians(double degrees) {
  constexpr double kPi = 3.14159265358979323846;
  return degrees * kPi / 180.0;
}

double haversineDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
  constexpr double kEarthRadiusMeters = 6371000.0;
  const double dLat = toRadians(lat2 - lat1);
  const double dLon = toRadians(lon2 - lon1);
  const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
                   std::cos(toRadians(lat1)) * std::cos(toRadians(lat2)) *
                       std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
  const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  return kEarthRadiusMeters * c;
}

}  // namespace

Scheduler::Scheduler(StateStore& stateStore, PayloadBuilder& payloadBuilder,
                     EndpointSender& endpointSender, LogFn logFn,
                     ResultFn resultFn)
    : stateStore_(stateStore),
      payloadBuilder_(payloadBuilder),
      endpointSender_(endpointSender),
      logFn_(std::move(logFn)),
      resultFn_(std::move(resultFn)) {}

Scheduler::~Scheduler() { stop(); }

void Scheduler::configure(const RuntimeConfig& config) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  const auto previousNextSendTimes = nextSendTimes_;
  config_ = config;
  const auto now = Clock::now();
  nextSendTimes_.clear();
  for (std::size_t i = 0; i < config_.endpoints.size(); ++i) {
    const auto key = endpointScheduleKey(i, config_.endpoints[i]);
    const auto existing = previousNextSendTimes.find(key);
    nextSendTimes_[key] =
        existing != previousNextSendTimes.end() ? existing->second : now;
  }
}

void Scheduler::start() {
  if (running_.exchange(true)) {
    return;
  }

  stopRequested_ = false;
  worker_ = std::thread(&Scheduler::runLoop, this);
}

void Scheduler::stop() {
  stopRequested_ = true;

  if (worker_.joinable()) {
    worker_.join();
  }

  running_ = false;
}

bool Scheduler::isRunning() const { return running_; }

std::size_t Scheduler::tick() { return tickAt(Clock::now()); }

bool Scheduler::isEndpointDue(
    const std::string& key, Clock::time_point now,
    const std::map<std::string, Clock::time_point>& nextSendTimesCopy,
    const EndpointConfig& /*endpoint*/) const {
  // No log on the not-due branch: it ran every second per endpoint and
  // buried the informative events (send start, succeeded/failed). The
  // scheduleNextSend log already tells you `next_due_in_ms` after each
  // successful send — that's enough for debugging timing.
  const auto nextSend = nextSendTimesCopy.find(key);
  return nextSend == nextSendTimesCopy.end() || now >= nextSend->second;
}

std::optional<std::pair<double, double>>
Scheduler::lookupLastSuccessfulPosition(const std::string& key) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  const auto it = lastSuccessfulPositions_.find(key);
  if (it == lastSuccessfulPositions_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool Scheduler::shouldSkipForMinDistance(const Snapshot& snapshot,
                                         const EndpointConfig& endpoint,
                                         const std::string& key) const {
  if (!snapshot.hasValidPosition()) {
    return false;
  }

  const int minDistanceMeters = effectiveMinDistanceMeters(endpoint);
  if (minDistanceMeters <= 0) {
    return false;
  }

  const auto lastSuccessful = lookupLastSuccessfulPosition(key);
  if (!lastSuccessful.has_value()) {
    return false;
  }

  const double distanceMeters = haversineDistanceMeters(
      lastSuccessful->first, lastSuccessful->second, *snapshot.lat,
      *snapshot.lon);
  if (distanceMeters >= static_cast<double>(minDistanceMeters)) {
    return false;
  }

  logMinDistanceSkip(endpoint, distanceMeters, minDistanceMeters);
  return true;
}

void Scheduler::logSendStart(const EndpointConfig& endpoint) const {
  if (!logFn_) {
    return;
  }

  std::ostringstream stream;
  stream << "1tracker_pi: endpoint '" << endpoint.name << "' send start"
         << ", now_wall=" << formatWallClockNow()
         << ", interval_minutes=" << effectiveSendIntervalMinutes(endpoint);
  logFn_(stream.str());
}

void Scheduler::logMinDistanceSkip(const EndpointConfig& endpoint,
                                   double distanceMeters,
                                   int minDistanceMeters) const {
  if (!logFn_) {
    return;
  }

  std::ostringstream stream;
  stream << "1tracker_pi: endpoint '" << endpoint.name
         << "' skip minimum distance"
         << ", now_wall=" << formatWallClockNow()
         << ", distance_meters=" << static_cast<long long>(distanceMeters + 0.5)
         << ", minimum_distance_meters=" << minDistanceMeters;
  logFn_(stream.str());
}

std::optional<std::string> Scheduler::buildPayloadForEndpoint(
    const Snapshot& snapshot, const EndpointConfig& endpoint) const {
  return payloadBuilder_.buildPayload(snapshot, endpoint);
}

void Scheduler::logMissingPayload(const EndpointConfig& endpoint) const {
  if (!logFn_) {
    return;
  }

  std::ostringstream stream;
  stream << "1tracker_pi: endpoint '" << endpoint.name << "' no valid payload"
         << ", now_wall=" << formatWallClockNow();
  logFn_(stream.str());
}

EndpointSender::Result Scheduler::sendPayload(const EndpointConfig& endpoint,
                                              const std::string& payload) const {
  return endpointSender_.send(endpoint, payload);
}

void Scheduler::logSendResult(
    const EndpointConfig& endpoint, const EndpointSender::Result& result,
    std::chrono::system_clock::time_point startedAt,
    std::chrono::system_clock::time_point finishedAt) const {
  if (!logFn_) {
    return;
  }

  std::ostringstream stream;
  stream << "1tracker_pi: endpoint '" << endpoint.name << "' "
         << (result.success ? "succeeded" : "failed")
         << ", header=" << endpoint.headerName << ": "
         << EndpointSender::MaskSecret(endpoint.headerValue)
         << ", detail=" << result.message
         << ", send_started_wall=" << formatWallClock(startedAt)
         << ", send_finished_wall=" << formatWallClock(finishedAt)
         << ", duration_ms=" << durationMillis(finishedAt - startedAt);
  logFn_(stream.str());
}

Scheduler::SendOutcome Scheduler::sendEndpoint(
    const Snapshot& snapshot, const EndpointConfig& endpoint) const {
  logSendStart(endpoint);

  const auto payload = buildPayloadForEndpoint(snapshot, endpoint);
  if (!payload.has_value()) {
    logMissingPayload(endpoint);
    return SendOutcome::NoPayload;
  }

  const auto sendStartedAt = std::chrono::system_clock::now();
  const auto result = sendPayload(endpoint, *payload);
  const auto sendFinishedAt = std::chrono::system_clock::now();
  if (resultFn_) {
    resultFn_(endpoint, result);
  }
  logSendResult(endpoint, result, sendStartedAt, sendFinishedAt);
  return result.success ? SendOutcome::Success : SendOutcome::SendFailed;
}

void Scheduler::scheduleNextSend(const std::string& key, Clock::time_point now,
                                 const EndpointConfig& endpoint) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  nextSendTimes_[key] =
      now + std::chrono::minutes(effectiveSendIntervalMinutes(endpoint));
  if (logFn_) {
    std::ostringstream stream;
    stream << "1tracker_pi: endpoint '" << endpoint.name
           << "' scheduled next send"
           << ", now_wall=" << formatWallClockNow()
           << ", next_due_in_ms=" << durationMillis(nextSendTimes_[key] - Clock::now())
           << ", interval_minutes=" << effectiveSendIntervalMinutes(endpoint);
    logFn_(stream.str());
  }
}

std::pair<RuntimeConfig, std::map<std::string, Scheduler::Clock::time_point>>
Scheduler::tickContext() const {
  RuntimeConfig configCopy;
  std::map<std::string, Clock::time_point> nextSendTimesCopy;
  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    configCopy = config_;
    nextSendTimesCopy = nextSendTimes_;
  }
  return {configCopy, nextSendTimesCopy};
}

std::size_t Scheduler::tickAt(Clock::time_point now) {
  const auto [configCopy, nextSendTimesCopy] = tickContext();

  if (!configCopy.enabled) {
    return 0;
  }

  const auto snapshot = stateStore_.getSnapshot();

  std::size_t sent = 0;
  for (std::size_t i = 0; i < configCopy.endpoints.size(); ++i) {
    const auto& endpoint = configCopy.endpoints[i];
    if (!endpoint.enabled) {
      continue;
    }

    const auto key = endpointScheduleKey(i, endpoint);
    if (!isEndpointDue(key, now, nextSendTimesCopy, endpoint)) {
      continue;
    }

    if (shouldSkipForMinDistance(snapshot, endpoint, key)) {
      scheduleNextSend(key, now, endpoint);
      continue;
    }

    const auto outcome = sendEndpoint(snapshot, endpoint);
    if (outcome == SendOutcome::NoPayload) {
      // Payload isn't buildable yet — typically a cold start before the
      // first GPS fix. Leave nextSendTimes_ as-is so the next 1 s tick
      // retries immediately once the fix arrives, instead of pushing
      // the first real send out by a full interval.
      continue;
    }
    if (outcome == SendOutcome::Success) {
      if (snapshot.hasValidPosition()) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        lastSuccessfulPositions_[key] = {*snapshot.lat, *snapshot.lon};
      }
      ++sent;
    }
    scheduleNextSend(key, now, endpoint);
  }

  return sent;
}

void Scheduler::runLoop() {
#ifdef _WIN32
  // Per-thread: translate Windows structured exceptions (access violations,
  // etc.) into C++ exceptions we can catch below. Otherwise an AV in any
  // downstream call (wxcurl, libcurl, DNS, jsoncpp) kills OCPN.
  _set_se_translator([](unsigned int code, EXCEPTION_POINTERS* info) {
    char buffer[256];
    void* faultIp = nullptr;
    unsigned long long badAddr = 0;
    const char* kind = "?";
    if (info != nullptr && info->ExceptionRecord != nullptr) {
      faultIp = info->ExceptionRecord->ExceptionAddress;
      if (info->ExceptionRecord->NumberParameters >= 2) {
        const auto op = info->ExceptionRecord->ExceptionInformation[0];
        badAddr = static_cast<unsigned long long>(
            info->ExceptionRecord->ExceptionInformation[1]);
        kind = (op == 0 ? "read" : op == 1 ? "write" : op == 8 ? "exec" : "?");
      }
    }
    std::snprintf(buffer, sizeof(buffer),
                  "Scheduler worker SEH 0x%08x at IP=%p kind=%s addr=0x%llx",
                  code, faultIp, kind, badAddr);
    throw std::runtime_error(buffer);
  });
#endif

  // Poll-based wait: sleep 100 ms, re-check stopRequested_, tick every
  // full second. Semantically equivalent to condition_variable::wait_for
  // but without the underlying std::mutex that condition_variable pulls
  // in — which, on VS 2022 17.10+ compiled against an older msvcp140.dll,
  // crashes every lock(). See include/1tracker_pi/state_store.h for the
  // full rationale.
  auto nextTickAt = Clock::now() + std::chrono::seconds(1);
  while (!stopRequested_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (stopRequested_) {
      break;
    }
    if (Clock::now() < nextTickAt) {
      continue;
    }
    nextTickAt = Clock::now() + std::chrono::seconds(1);

    try {
      tick();
    } catch (const std::exception& error) {
      if (logFn_) {
        logFn_(std::string("1tracker_pi: scheduler tick threw: ") +
               error.what());
      }
    } catch (...) {
      if (logFn_) {
        logFn_("1tracker_pi: scheduler tick threw unknown exception");
      }
    }
  }
}

}  // namespace tracker_pi
