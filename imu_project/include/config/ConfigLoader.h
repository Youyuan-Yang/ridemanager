#pragma once

#include <string>

#include "config/AppConfig.h"

namespace imu {

class ConfigLoader {
public:
    static AppConfig load(const std::string& path);

private:
    static void applyValue(
        AppConfig& config,
        const std::string& section,
        const std::string& key,
        const std::string& value);
};

}  // namespace imu
