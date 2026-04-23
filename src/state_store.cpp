#include "1tracker_pi/state_store.h"

#include <cmath>
#include <mutex>

namespace tracker_pi {

void StateStore::updateTimevalue(std::int64_t timevalue) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  snapshot_.timevalue = timevalue;
}

void StateStore::updateLatLon(double lat, double lon) {
  if (!std::isfinite(lat) || !std::isfinite(lon) ||
      std::abs(lat) > 90.0 || std::abs(lon) > 180.0) {
    return;
  }
  std::lock_guard<std::shared_mutex> lock(mutex_);
  snapshot_.lat = lat;
  snapshot_.lon = lon;
}

void StateStore::updateAWA(double awa) {
  if (!std::isfinite(awa) || awa < 0.0 || awa > 360.0) {
    return;
  }
  std::lock_guard<std::shared_mutex> lock(mutex_);
  snapshot_.awa = awa;
}

void StateStore::updateAWS(double aws) {
  if (!std::isfinite(aws) || aws < 0.0) {
    return;
  }
  std::lock_guard<std::shared_mutex> lock(mutex_);
  snapshot_.aws = aws;
}

bool StateStore::hasValidPosition() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return snapshot_.hasValidPosition();
}

Snapshot StateStore::getSnapshot() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return snapshot_;
}

}  // namespace tracker_pi
