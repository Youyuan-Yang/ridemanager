#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace imu {

inline std::string formatTimestampUtc(std::chrono::system_clock::time_point timePoint) {
    const auto time = std::chrono::system_clock::to_time_t(timePoint);
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                        timePoint.time_since_epoch()) %
                    1000000;

    std::tm tm{};
    gmtime_r(&time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(6) << std::setfill('0') << us.count() << "+00";
    return oss.str();
}

}  // namespace imu
