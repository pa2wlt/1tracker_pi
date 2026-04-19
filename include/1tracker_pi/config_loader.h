#pragma once

#include <filesystem>
#include <string>

#include "1tracker_pi/runtime_config.h"

namespace tracker_pi {

class ConfigLoader {
public:
  RuntimeConfig loadFromFile(const std::filesystem::path& path) const;
  RuntimeConfig loadFromString(const std::string& jsonText) const;
};

}  // namespace tracker_pi
