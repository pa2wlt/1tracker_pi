#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "1tracker_pi/state_store.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  tracker_pi::StateStore store;
  expect(!store.hasValidPosition(), "position should start invalid");

  store.updateTimevalue(1710000000);
  store.updateLatLon(52.12345, 4.98765);
  store.updateAWA(38.2);
  store.updateAWS(14.6);

  const auto snapshot = store.getSnapshot();
  expect(snapshot.hasValidPosition(), "position should be valid");
  expect(snapshot.hasTimestamp(), "timestamp should be present");
  expect(snapshot.awa.has_value() && *snapshot.awa == 38.2, "awa mismatch");
  expect(snapshot.aws.has_value() && *snapshot.aws == 14.6, "aws mismatch");

  return EXIT_SUCCESS;
}
