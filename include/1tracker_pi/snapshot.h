#pragma once

#include <cstdint>
#include <optional>

namespace tracker_pi {

struct Snapshot {
  std::optional<std::int64_t> timevalue;
  std::optional<double> lat;
  std::optional<double> lon;
  std::optional<double> awa;
  std::optional<double> aws;

  bool hasValidPosition() const { return lat.has_value() && lon.has_value(); }
  bool hasTimestamp() const { return timevalue.has_value(); }
};

}  // namespace tracker_pi
