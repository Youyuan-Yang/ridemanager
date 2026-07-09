#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "algorithm/LowPassFilter.h"
#include "algorithm/MadgwickAHRS.h"
#include "common/BlockingQueue.h"
#include "common/IMUData.h"
#include "config/AppConfig.h"
#include "database/PostgreSQLManager.h"
#include "driver/MPU6050.h"

namespace imu {

class IMUService {
public:
    explicit IMUService(AppConfig config);
    ~IMUService();

    IMUService(const IMUService&) = delete;
    IMUService& operator=(const IMUService&) = delete;

    void start();
    void stop();
    bool running() const;

private:
    void acquisitionLoop();
    void processingLoop();
    void databaseLoop();
    void calibrateSensor();

    ProcessedIMUData processSample(const RawIMUData& raw, double dtSeconds);

    AppConfig config_;
    MPU6050 imu_;
    VectorLowPassFilter accelFilter_;
    VectorLowPassFilter gyroFilter_;
    MadgwickAHRS ahrs_;
    Vector3 gyroBiasDps_{};

    BlockingQueue<RawIMUData> rawQueue_;
    BlockingQueue<ProcessedIMUData> processedQueue_;
    std::unique_ptr<PostgreSQLManager> database_;

    std::atomic_bool running_{false};
    std::thread acquisitionThread_;
    std::thread processingThread_;
    std::thread databaseThread_;
};

}  // namespace imu
