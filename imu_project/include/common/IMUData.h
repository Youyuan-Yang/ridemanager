#pragma once

#include <chrono>
#include <string>

namespace imu {

struct Vector3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct Quaternion {
    double q0{1.0};
    double q1{0.0};
    double q2{0.0};
    double q3{0.0};
};

struct EulerAngles {
    double roll{0.0};
    double pitch{0.0};
    double yaw{0.0};
};

enum class SensorStatus {
    Ok,
    I2cError,
    InvalidData
};

inline const char* toString(SensorStatus status) {
    switch (status) {
        case SensorStatus::Ok:
            return "OK";
        case SensorStatus::I2cError:
            return "I2C_ERROR";
        case SensorStatus::InvalidData:
            return "INVALID_DATA";
    }
    return "UNKNOWN";
}

struct RawIMUData {
    std::chrono::system_clock::time_point observedAt{};
    Vector3 accelerationG{};
    Vector3 gyroscopeDps{};
    double temperatureC{0.0};
    SensorStatus status{SensorStatus::Ok};
};

struct ProcessedIMUData {
    std::chrono::system_clock::time_point observedAt{};
    Vector3 accelerationG{};
    Vector3 gyroscopeDps{};
    double temperatureC{0.0};
    Quaternion quaternion{};
    EulerAngles eulerDeg{};
    SensorStatus status{SensorStatus::Ok};
};

}  // namespace imu
