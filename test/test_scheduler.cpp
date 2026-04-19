#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdlib>
#include <stdexcept>
#include <string>

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

  return EXIT_SUCCESS;
}
