#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

#include "1tracker_pi/endpoint_sender.h"
#include "1tracker_pi/payload_builder.h"
#include "1tracker_pi/runtime_config.h"
#include "1tracker_pi/state_store.h"

namespace tracker_pi {

class Scheduler {
public:
  using LogFn = std::function<void(const std::string&)>;
  using ResultFn =
      std::function<void(const EndpointConfig&, const EndpointSender::Result&)>;

  Scheduler(StateStore& stateStore, PayloadBuilder& payloadBuilder,
            EndpointSender& endpointSender, LogFn logFn = {},
            ResultFn resultFn = {});
  ~Scheduler();

  void configure(const RuntimeConfig& config);
  void start();
  void stop();
  bool isRunning() const;
  std::size_t tick();

private:
  using Clock = std::chrono::steady_clock;

  std::size_t tickAt(Clock::time_point now);
  bool isEndpointDue(const std::string& key, Clock::time_point now,
                     const std::map<std::string, Clock::time_point>& nextSendTimesCopy,
                     const EndpointConfig& endpoint) const;
  bool shouldSkipForMinDistance(const Snapshot& snapshot, const EndpointConfig& endpoint,
                                const std::string& key) const;
  void logSendStart(const EndpointConfig& endpoint) const;
  void logMinDistanceSkip(const EndpointConfig& endpoint, double distanceMeters,
                          int minDistanceMeters) const;
  std::optional<std::string> buildPayloadForEndpoint(
      const Snapshot& snapshot, const EndpointConfig& endpoint) const;
  void logMissingPayload(const EndpointConfig& endpoint) const;
  EndpointSender::Result sendPayload(const EndpointConfig& endpoint,
                                     const std::string& payload) const;
  void logSendResult(const EndpointConfig& endpoint,
                     const EndpointSender::Result& result,
                     std::chrono::system_clock::time_point startedAt,
                     std::chrono::system_clock::time_point finishedAt) const;
  bool sendEndpoint(const Snapshot& snapshot, const EndpointConfig& endpoint) const;
  std::pair<RuntimeConfig, std::map<std::string, Clock::time_point>>
  tickContext() const;
  void scheduleNextSend(const std::string& key, Clock::time_point now,
                        const EndpointConfig& endpoint);
  void runLoop();

  StateStore& stateStore_;
  PayloadBuilder& payloadBuilder_;
  EndpointSender& endpointSender_;
  LogFn logFn_;
  ResultFn resultFn_;

  mutable std::mutex mutex_;
  std::condition_variable condition_;
  RuntimeConfig config_;
  std::map<std::string, Clock::time_point> nextSendTimes_;
  std::map<std::string, std::pair<double, double>> lastSuccessfulPositions_;
  std::thread worker_;
  std::atomic<bool> stopRequested_{false};
  std::atomic<bool> running_{false};
};

}  // namespace tracker_pi
