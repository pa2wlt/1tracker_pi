#pragma once

#include <cstdint>
#include <mutex>

#include "1tracker_pi/snapshot.h"

namespace tracker_pi {

class StateStore {
public:
  void updateTimevalue(std::int64_t timevalue);
  void updateLatLon(double lat, double lon);
  void updateAWA(double awa);
  void updateAWS(double aws);

  bool hasValidPosition() const;
  Snapshot getSnapshot() const;

private:
  mutable std::mutex mutex_;
  Snapshot snapshot_;
};

}  // namespace tracker_pi
