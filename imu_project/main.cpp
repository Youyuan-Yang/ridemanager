#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "common/Logger.h"
#include "common/Uuid.h"
#include "config/ConfigLoader.h"
#include "service/IMUService.h"

namespace {

std::atomic_bool gStopRequested{false};

void handleSignal(int) {
    gStopRequested = true;
}

void printUsage(const char* programName) {
    std::cout
        << "Usage:\n"
        << "  " << programName << " [--config config/config.yaml]\n\n"
        << "Default config path:\n"
        << "  config/config.yaml\n";
}

std::string parseConfigPath(int argc, char* argv[]) {
    std::string path = "config/config.yaml";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }

        if (arg == "--config") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--config requires a path");
            }
            path = argv[++i];
            continue;
        }

        throw std::invalid_argument("unknown argument: " + arg);
    }

    return path;
}

void validateConfig(const imu::AppConfig& config) {
    if (!imu::isValidUuid(config.deviceId)) {
        throw std::invalid_argument("device_id must be a valid UUID");
    }

    if (config.safetyDecisionId.has_value() &&
        !imu::isValidUuid(config.safetyDecisionId.value())) {
        throw std::invalid_argument("safety_decision_id must be empty or a valid UUID");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        const std::string configPath = parseConfigPath(argc, argv);
        imu::AppConfig config = imu::ConfigLoader::load(configPath);
        validateConfig(config);

        imu::Logger::info("Starting IMU service with config: " + configPath);
        imu::IMUService service(config);
        service.start();

        while (!gStopRequested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        service.stop();
        return 0;
    } catch (const std::exception& ex) {
        imu::Logger::error(std::string("Fatal error: ") + ex.what());
        return 1;
    }
}
