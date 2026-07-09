#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace imu {

enum class LogLevel {
    Info,
    Warn,
    Error
};

class Logger {
public:
    static void info(const std::string& message) {
        log(LogLevel::Info, "INFO", message);
    }

    static void warn(const std::string& message) {
        log(LogLevel::Warn, "WARN", message);
    }

    static void error(const std::string& message) {
        log(LogLevel::Error, "ERROR", message);
    }

private:
    static std::mutex& mutex() {
        static std::mutex instance;
        return instance;
    }

    static std::string timestamp() {
        const auto now = std::chrono::system_clock::now();
        const auto nowTime = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;

        std::tm tm{};
        localtime_r(&nowTime, &tm);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
            << std::setw(3) << std::setfill('0') << ms.count();
        return oss.str();
    }

    static void log(LogLevel, const char* levelName, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex());
        std::cout << '[' << timestamp() << "] [" << levelName << "] [tid="
                  << std::this_thread::get_id() << "] " << message << '\n';
    }
};

}  // namespace imu
