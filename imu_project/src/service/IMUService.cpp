#include "service/IMUService.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "common/Logger.h"

namespace imu {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::chrono::nanoseconds periodFromHz(double hz) {
    if (hz <= 0.0) {
        throw std::invalid_argument("frequency must be positive");
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(1.0 / hz));
}

double degreesToRadians(double degrees) {
    return degrees * kPi / 180.0;
}

bool isValidAxisSign(int sign) {
    return sign == -1 || sign == 1;
}

Vector3 applyMountingTransform(const Vector3& value, const AppConfig& config, double scale) {
    return Vector3{
        static_cast<double>(config.mountXSign) * value.x * scale,
        static_cast<double>(config.mountYSign) * value.y * scale,
        static_cast<double>(config.mountZSign) * value.z * scale,
    };
}

std::string vectorLine(const char* label, const Vector3& value, const char* unit) {
    std::ostringstream oss;
    oss << label << ": " << std::fixed << std::setprecision(4)
        << value.x << ' ' << value.y << ' ' << value.z << ' ' << unit;
    return oss.str();
}

std::string poseLine(const ProcessedIMUData& data) {
    std::ostringstream oss;
    oss << "姿态: roll=" << std::fixed << std::setprecision(3) << data.eulerDeg.roll
        << " pitch=" << data.eulerDeg.pitch
        << " yaw=" << data.eulerDeg.yaw
        << " degree q=(" << data.quaternion.q0 << ", "
        << data.quaternion.q1 << ", " << data.quaternion.q2 << ", "
        << data.quaternion.q3 << ")";
    return oss.str();
}

}  // namespace

IMUService::IMUService(AppConfig config)
    : config_(std::move(config)),
      imu_(config_.i2cDevice, config_.address),
      accelFilter_(config_.lowpassAlpha),
      gyroFilter_(config_.lowpassAlpha),
      ahrs_(config_.madgwickBeta),
      rawQueue_(config_.queueCapacity),
      processedQueue_(config_.queueCapacity) {
    if (!isValidAxisSign(config_.mountXSign) ||
        !isValidAxisSign(config_.mountYSign) ||
        !isValidAxisSign(config_.mountZSign)) {
        throw std::invalid_argument("mount axis signs must be either 1 or -1");
    }

    if (config_.accelScaleCorrection <= 0.0) {
        throw std::invalid_argument("accel_scale_correction must be positive");
    }
}

IMUService::~IMUService() {
    stop();
}

void IMUService::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    try {
        imu_.init();
        calibrateSensor();

        if (config_.enableDatabase) {
            database_ = std::make_unique<PostgreSQLManager>(config_.databaseConninfo);
            try {
                database_->connect();
            } catch (const std::exception& ex) {
                Logger::warn(std::string("PostgreSQL initial connection failed: ") + ex.what());
            }
        }

        acquisitionThread_ = std::thread(&IMUService::acquisitionLoop, this);
        processingThread_ = std::thread(&IMUService::processingLoop, this);
        databaseThread_ = std::thread(&IMUService::databaseLoop, this);
    } catch (...) {
        running_ = false;
        throw;
    }
}

void IMUService::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    rawQueue_.close();
    processedQueue_.close();

    if (acquisitionThread_.joinable()) {
        acquisitionThread_.join();
    }
    if (processingThread_.joinable()) {
        processingThread_.join();
    }
    if (databaseThread_.joinable()) {
        databaseThread_.join();
    }

    Logger::info("IMU service stopped");
}

bool IMUService::running() const {
    return running_;
}

void IMUService::acquisitionLoop() {
    const auto samplePeriod = periodFromHz(config_.sampleRateHz);
    auto nextWakeup = std::chrono::steady_clock::now();

    while (running_) {
        nextWakeup += samplePeriod;

        try {
            rawQueue_.push(imu_.read());
        } catch (const std::exception& ex) {
            Logger::error(std::string("MPU6050 read failed: ") + ex.what());
        }

        std::this_thread::sleep_until(nextWakeup);
        if (std::chrono::steady_clock::now() > nextWakeup + samplePeriod) {
            nextWakeup = std::chrono::steady_clock::now();
        }
    }
}

void IMUService::processingLoop() {
    const double fallbackDt = 1.0 / config_.sampleRateHz;
    bool hasLastTimestamp = false;
    std::chrono::system_clock::time_point lastTimestamp{};

    while (running_) {
        RawIMUData raw;
        if (!rawQueue_.waitPop(raw, std::chrono::milliseconds(100))) {
            continue;
        }

        double dtSeconds = fallbackDt;
        if (hasLastTimestamp) {
            dtSeconds = std::chrono::duration<double>(raw.observedAt - lastTimestamp).count();
            if (dtSeconds <= 0.0 || dtSeconds > 0.2) {
                dtSeconds = fallbackDt;
            }
        }

        lastTimestamp = raw.observedAt;
        hasLastTimestamp = true;

        try {
            processedQueue_.push(processSample(raw, dtSeconds));
        } catch (const std::exception& ex) {
            Logger::error(std::string("IMU algorithm processing failed: ") + ex.what());
        }
    }
}

void IMUService::databaseLoop() {
    const auto writePeriod = periodFromHz(config_.databaseRateHz);
    auto nextWrite = std::chrono::steady_clock::now() + writePeriod;

    while (running_) {
        std::this_thread::sleep_until(nextWrite);

        std::vector<ProcessedIMUData> batch;
        ProcessedIMUData item;
        while (processedQueue_.tryPop(item)) {
            batch.push_back(item);
        }

        if (!batch.empty()) {
            const ProcessedIMUData& latest = batch.back();
            Logger::info(vectorLine("ACC", latest.accelerationG, "g"));
            Logger::info(vectorLine("GYRO", latest.gyroscopeDps, "deg/s"));
            Logger::info("TEMP: " + std::to_string(latest.temperatureC) + " C");
            Logger::info(poseLine(latest));

            if (config_.enableDatabase && database_) {
                try {
                    database_->insertBatch(
                        config_.safetyDecisionId,
                        config_.deviceId,
                        config_.sensorName,
                        batch);
                } catch (const std::exception& ex) {
                    Logger::error(std::string("Database insert failed: ") + ex.what());
                }
            }
        }

        nextWrite += writePeriod;
        if (std::chrono::steady_clock::now() > nextWrite + writePeriod) {
            nextWrite = std::chrono::steady_clock::now() + writePeriod;
        }
    }
}

void IMUService::calibrateSensor() {
    if (!config_.calibrationEnabled) {
        Logger::warn("Gyroscope startup zero-bias calibration disabled");
        return;
    }

    if (config_.calibrationSamples <= 0) {
        throw std::invalid_argument("calibration_samples must be positive");
    }

    Logger::info("Keep vehicle still, calibrating gyroscope zero bias...");

    Vector3 accelSum{};
    Vector3 accelSquareSum{};
    Vector3 gyroSum{};
    Vector3 gyroSquareSum{};
    const auto samplePeriod = periodFromHz(config_.sampleRateHz);

    for (int i = 0; i < config_.calibrationSamples; ++i) {
        const RawIMUData raw = imu_.read();
        const Vector3 vehicleAccel = applyMountingTransform(
            raw.accelerationG,
            config_,
            config_.accelScaleCorrection);
        const Vector3 vehicleGyro = applyMountingTransform(raw.gyroscopeDps, config_, 1.0);

        accelSum.x += vehicleAccel.x;
        accelSum.y += vehicleAccel.y;
        accelSum.z += vehicleAccel.z;
        accelSquareSum.x += vehicleAccel.x * vehicleAccel.x;
        accelSquareSum.y += vehicleAccel.y * vehicleAccel.y;
        accelSquareSum.z += vehicleAccel.z * vehicleAccel.z;
        gyroSum.x += vehicleGyro.x;
        gyroSum.y += vehicleGyro.y;
        gyroSum.z += vehicleGyro.z;
        gyroSquareSum.x += vehicleGyro.x * vehicleGyro.x;
        gyroSquareSum.y += vehicleGyro.y * vehicleGyro.y;
        gyroSquareSum.z += vehicleGyro.z * vehicleGyro.z;
        std::this_thread::sleep_for(samplePeriod);
    }

    const double count = static_cast<double>(config_.calibrationSamples);
    const Vector3 accelMean{
        accelSum.x / count,
        accelSum.y / count,
        accelSum.z / count,
    };
    gyroBiasDps_ = Vector3{
        gyroSum.x / count,
        gyroSum.y / count,
        gyroSum.z / count,
    };

    const auto stddev = [count](double sum, double squareSum) {
        const double mean = sum / count;
        const double variance = squareSum / count - mean * mean;
        return std::sqrt(std::max(variance, 0.0));
    };

    const Vector3 accelStddev{
        stddev(accelSum.x, accelSquareSum.x),
        stddev(accelSum.y, accelSquareSum.y),
        stddev(accelSum.z, accelSquareSum.z),
    };
    const Vector3 gyroStddev{
        stddev(gyroSum.x, gyroSquareSum.x),
        stddev(gyroSum.y, gyroSquareSum.y),
        stddev(gyroSum.z, gyroSquareSum.z),
    };

    const double maxAccelStddev = std::max({accelStddev.x, accelStddev.y, accelStddev.z});
    const double maxGyroStddev = std::max({gyroStddev.x, gyroStddev.y, gyroStddev.z});
    const double accelNorm = std::sqrt(
        accelMean.x * accelMean.x +
        accelMean.y * accelMean.y +
        accelMean.z * accelMean.z);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(5);

    if (maxAccelStddev > config_.calibrationMaxAccelStddevG ||
        maxGyroStddev > config_.calibrationMaxGyroStddevDps) {
        gyroBiasDps_ = Vector3{};
        oss << "Gyroscope calibration skipped: vehicle is not static enough, "
            << "max_accel_stddev=" << maxAccelStddev << "g "
            << "max_gyro_stddev=" << maxGyroStddev << "deg/s";
        Logger::warn(oss.str());
        return;
    }

    oss << "Gyroscope calibration done, gyro_bias=("
        << gyroBiasDps_.x << ", " << gyroBiasDps_.y << ", " << gyroBiasDps_.z
        << ")deg/s accel_norm_at_start=" << accelNorm
        << "g. Accelerometer startup bias is not calibrated.";
    Logger::info(oss.str());
}

ProcessedIMUData IMUService::processSample(const RawIMUData& raw, double dtSeconds) {
    const Vector3 correctedAccel = applyMountingTransform(
        raw.accelerationG,
        config_,
        config_.accelScaleCorrection);
    const Vector3 vehicleGyro = applyMountingTransform(raw.gyroscopeDps, config_, 1.0);
    const Vector3 correctedGyro{
        vehicleGyro.x - gyroBiasDps_.x,
        vehicleGyro.y - gyroBiasDps_.y,
        vehicleGyro.z - gyroBiasDps_.z,
    };

    const Vector3 filteredAccel = accelFilter_.update(correctedAccel);
    const Vector3 filteredGyro = gyroFilter_.update(correctedGyro);

    const Vector3 gyroRadPerSec{
        degreesToRadians(filteredGyro.x),
        degreesToRadians(filteredGyro.y),
        degreesToRadians(filteredGyro.z),
    };

    ahrs_.update(filteredAccel, gyroRadPerSec, dtSeconds);

    ProcessedIMUData processed;
    processed.observedAt = raw.observedAt;
    processed.accelerationG = filteredAccel;
    processed.gyroscopeDps = filteredGyro;
    processed.temperatureC = raw.temperatureC;
    processed.quaternion = ahrs_.quaternion();
    processed.eulerDeg = ahrs_.eulerDegrees();
    processed.status = raw.status;
    return processed;
}

}  // namespace imu
