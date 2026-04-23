#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/nfl_settings.h"
#include "1tracker_pi/scheduler.h"

namespace {

class FakeSender final : public tracker_pi::EndpointSender {
public:
  Result send(const tracker_pi::EndpointConfig& endpoint,
              const std::string& payload) const override {
    (void)endpoint;
    (void)payload;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      ++sendCount_;
    }
    condition_.notify_all();

    return Result{true, 200, "ok"};
  }

  bool waitForSends(std::size_t expected,
                    std::chrono::milliseconds timeout) const {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, timeout,
                               [this, expected] { return sendCount_ >= expected; });
  }

  std::size_t sendCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sendCount_;
  }

  void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    sendCount_ = 0;
  }

private:
  mutable std::mutex mutex_;
  mutable std::condition_variable condition_;
  mutable std::size_t sendCount_ = 0;
};

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"main", "http_json_with_header_key", true, true, 1, 60,
         "https://example.com", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-main";

    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);

    expect(scheduler.tick() == 1, "one endpoint should have been sent");

    config.endpoints.front().name = "renamed-main";
    config.endpoints.front().type = tracker_pi::kEndpointTypeNoForeignLand;
    config.endpoints.front().sendIntervalMinutes = 0;
    config.endpoints.front().headerValue =
        "424534f5-13bc-42e8-ad02-33f9e27f7750";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());
    scheduler.configure(config);
    scheduler.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    scheduler.stop();
    expect(sender.sendCount() == 1,
           "reconfiguring an existing endpoint should preserve its next send time");
  }

  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"nfl-main", tracker_pi::kEndpointTypeNoForeignLand, true, false, 0, 0,
         tracker_pi::nfl::trackingUrl(), 10,
         "X-NFL-API-Key", "424534f5-13bc-42e8-ad02-33f9e27f7750"});
    config.endpoints.front().id = "endpoint-nfl";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);

    expect(scheduler.tick() == 1, "NFL endpoint should send immediately once");
    scheduler.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    scheduler.stop();
    expect(sender.sendCount() == 1,
           "NFL interval clamp should prevent another send within 1.5 seconds");
  }

  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"main", "http_json_with_header_key", true, true, 1, 60,
         "https://example.com", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-min-distance";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);
    expect(scheduler.tick() == 1, "first endpoint send should succeed");

    config.endpoints.front().sendIntervalMinutes = 1;
    config.endpoints.front().minDistanceMeters = 60;
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());
    scheduler.configure(config);

    sender.reset();
    store.updateTimevalue(1710000010);
    store.updateLatLon(52.12346, 4.98766);
    expect(scheduler.tick() == 0,
           "endpoint should skip when the new position is within minimum distance");
    expect(sender.sendCount() == 0,
           "sender should not be called for positions within minimum distance");
  }

  // Disabled runtime config: tick returns 0, nothing is sent
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = false;
    config.endpoints.push_back(
        {"main", "http_json_with_header_key", true, true, 1, 60,
         "https://example.com", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-disabled-config";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);
    expect(scheduler.tick() == 0, "disabled config must not send anything");
    expect(sender.sendCount() == 0, "sender must not be called when disabled");
  }

  // Disabled endpoint inside an enabled config: skipped
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"off", "http_json_with_header_key", false, false, 1, 60,
         "https://example.com/off", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-off";
    config.endpoints.push_back(
        {"on", "http_json_with_header_key", true, false, 1, 60,
         "https://example.com/on", 10, "X-API-Key", "SECRET"});
    config.endpoints.back().id = "endpoint-on";
    for (auto& endpoint : config.endpoints) {
      tracker_pi::normalizeEndpointConfig(endpoint);
    }

    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);
    expect(scheduler.tick() == 1, "only enabled endpoint should send");
    expect(sender.sendCount() == 1, "exactly one send");
  }

  // Missing timestamp in store: payload can't be built, tick returns 0
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"main", "http_json_with_header_key", true, true, 1, 60,
         "https://example.com", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-no-ts";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    // position set, but no timestamp -> payload empty
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);
    expect(scheduler.tick() == 0,
           "endpoint without timestamp must not send (payload unavailable)");
  }

  // Cold-start: first tick has no GPS fix yet. Must not push nextSendTime
  // out by a full interval — once the fix arrives, the very next tick
  // should send.
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"main", "http_json_with_header_key", true, true, 10, 60,
         "https://example.com", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-cold-start";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);

    expect(scheduler.tick() == 0,
           "first tick before any GPS fix must not send");

    // GPS fix arrives one tick later.
    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    expect(scheduler.tick() == 1,
           "send must happen on the first tick after the GPS fix arrives, "
           "not a full interval later");
  }

  // Activating a previously-disabled endpoint while the GPS fix is still
  // missing must not push the first send out by a full interval either —
  // once the fix arrives, the next tick should send.
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"main", "http_json_with_header_key", false, true, 10, 60,
         "https://example.com", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-activate";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);
    expect(scheduler.tick() == 0, "disabled endpoint must not send");

    // User flips it on while there is still no GPS fix.
    config.endpoints.front().enabled = true;
    scheduler.configure(config);
    expect(scheduler.tick() == 0,
           "activated endpoint without a fix must not send yet");

    // Fix arrives.
    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);
    expect(scheduler.tick() == 1,
           "activated endpoint must send on the first tick after the fix");
  }

  // Full log coverage: wire a logFn and exercise success, not-due, min-distance paths
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    std::vector<std::string> logs;
    std::mutex logMutex;
    auto logFn = [&](const std::string& msg) {
      std::lock_guard<std::mutex> lock(logMutex);
      logs.push_back(msg);
    };

    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"logged", "http_json_with_header_key", true, true, 1, 60,
         "https://example.com/logged", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-logged";
    config.endpoints.front().minDistanceMeters = 60;
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender, logFn);
    scheduler.configure(config);
    expect(!scheduler.isRunning(), "scheduler not started yet");

    // First tick: sends + logs send-start, send-result, scheduleNextSend
    expect(scheduler.tick() == 1, "logged first send");

    // Second tick at the same instant: not due → logs skip-not-due
    expect(scheduler.tick() == 0, "second immediate tick must not re-send");

    // Advance position slightly → within min-distance → logs min-distance-skip
    store.updateLatLon(52.12346, 4.98766);
    const auto farFuture = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    expect(scheduler.tickAt(farFuture) == 0, "within min-distance should skip");

    // Assert we got at least one of each log flavor
    auto hasLog = [&](const std::string& needle) {
      std::lock_guard<std::mutex> lock(logMutex);
      for (const auto& msg : logs) {
        if (msg.find(needle) != std::string::npos) return true;
      }
      return false;
    };
    expect(hasLog("send start"), "missing send-start log");
    expect(hasLog("succeeded"), "missing send-result log");
    expect(hasLog("scheduled next send"), "missing scheduleNextSend log");
    expect(hasLog("skip minimum distance"), "missing min-distance log");
  }

  // Failing sender: scheduler should still count it in the loop but with different behavior
  // (sendEndpoint returns false, nextSend still scheduled, lastSuccessfulPositions not updated)
  {
    class FailingSender final : public tracker_pi::EndpointSender {
    public:
      Result send(const tracker_pi::EndpointConfig&, const std::string&) const override {
        return Result{false, 500, "server error"};
      }
    };
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FailingSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"fail", "http_json_with_header_key", true, true, 1, 60,
         "https://example.com/fail", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-fail";
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);
    expect(scheduler.tick() == 0,
           "failing sender means tick returns 0 (no successful sends)");
  }

  // Min-distance guard does NOT skip when no previous successful position
  // (first-send path) — already exercised by the distance test above's first tick
  // but make it explicit with distance >= threshold on the second tick
  {
    tracker_pi::StateStore store;
    tracker_pi::PayloadBuilder payloadBuilder;
    FakeSender sender;
    tracker_pi::RuntimeConfig config;
    config.enabled = true;
    config.endpoints.push_back(
        {"main", "http_json_with_header_key", true, true, 1, 60,
         "https://example.com", 10, "X-API-Key", "SECRET"});
    config.endpoints.front().id = "endpoint-far";
    config.endpoints.front().minDistanceMeters = 60;
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());

    store.updateTimevalue(1710000000);
    store.updateLatLon(52.12345, 4.98765);

    tracker_pi::Scheduler scheduler(store, payloadBuilder, sender);
    scheduler.configure(config);
    expect(scheduler.tick() == 1, "first send stores position");

    // Move ~200m away (roughly 0.0018 degrees lat)
    store.updateLatLon(52.12530, 4.98765);
    config.endpoints.front().sendIntervalMinutes = 1;
    tracker_pi::normalizeEndpointConfig(config.endpoints.front());
    scheduler.configure(config);

    sender.reset();
    // Skip interval by using tickAt with a future time
    const auto farFuture = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    expect(scheduler.tickAt(farFuture) == 1,
           "endpoint should send when position moved beyond min-distance");
  }

  return EXIT_SUCCESS;
}
