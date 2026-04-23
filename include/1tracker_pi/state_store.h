#pragma once

#include <cstdint>
#include <shared_mutex>

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
  // std::shared_mutex (SRWLOCK-backed on Windows) instead of std::mutex:
  // on some MSVC/CRT combinations std::mutex's internal _Mutex_t pointer
  // can be null after construction and every lock() crashes with an
  // access-violation at addr=0x0. std::shared_mutex uses InitializeSRWLock
  // directly and has no such indirection.
  mutable std::shared_mutex mutex_;
  Snapshot snapshot_;
};

}  // namespace tracker_pi
