#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace imu {

struct AppConfig {
    std::string i2cDevice{"/dev/i2c-4"};
    int address{0x68};
    double sampleRateHz{100.0};
    double databaseRateHz{10.0};
    double madgwickBeta{0.1};
    double lowpassAlpha{0.1};
    bool calibrationEnabled{true};
    int calibrationSamples{200};
    double calibrationMaxGyroStddevDps{0.3};
    double calibrationMaxAccelStddevG{0.05};
    int mountXSign{1};
    int mountYSign{-1};
    int mountZSign{-1};
    double accelScaleCorrection{0.6766};

    bool enableDatabase{true};
    std::string databaseConninfo{
        "host=127.0.0.1 port=5432 dbname=vehicle user=postgres password=postgres connect_timeout=3"};
    std::string deviceId{"00000000-0000-0000-0000-000000000001"};
    std::optional<std::string> safetyDecisionId{};
    std::string sensorName{"GYRO"};
    std::size_t queueCapacity{512};
};

}  // namespace imu
