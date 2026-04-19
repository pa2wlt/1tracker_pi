#include "1tracker_pi/state_store.h"

namespace tracker_pi {

void StateStore::updateTimevalue(std::int64_t timevalue) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.timevalue = timevalue;
}

void StateStore::updateLatLon(double lat, double lon) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.lat = lat;
  snapshot_.lon = lon;
}

void StateStore::updateAWA(double awa) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.awa = awa;
}

void StateStore::updateAWS(double aws) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.aws = aws;
}

bool StateStore::hasValidPosition() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_.hasValidPosition();
}

Snapshot StateStore::getSnapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

}  // namespace tracker_pi
