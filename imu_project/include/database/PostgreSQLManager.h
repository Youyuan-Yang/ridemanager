#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "common/IMUData.h"

struct pg_conn;

namespace imu {

class PostgreSQLManager {
public:
    explicit PostgreSQLManager(std::string conninfo);
    ~PostgreSQLManager();

    PostgreSQLManager(const PostgreSQLManager&) = delete;
    PostgreSQLManager& operator=(const PostgreSQLManager&) = delete;

    void connect();
    void insertBatch(
        const std::optional<std::string>& safetyDecisionId,
        const std::string& deviceId,
        const std::string& sensorName,
        const std::vector<ProcessedIMUData>& batch);

private:
    void connectLocked();
    void disconnectLocked() noexcept;
    void ensureConnectedLocked();
    void prepareStatementsLocked();
    void execSimpleLocked(const std::string& sql);

    void insertSafetyDecisionLocked(
        const std::string& safetyDecisionId,
        const std::string& sensorName,
        const std::vector<ProcessedIMUData>& batch);

    void insertSnapshotLocked(
        const std::string& snapshotId,
        const std::string& safetyDecisionId,
        const std::string& deviceId,
        const std::string& sensorName,
        const ProcessedIMUData& data);

    void insertReadingLocked(
        const std::string& readingId,
        const std::string& snapshotId,
        const std::string& metric,
        double value,
        const std::string& unit);

    bool connectionBadLocked() const;

    std::string conninfo_;
    pg_conn* conn_{nullptr};
    bool prepared_{false};
    std::mutex mutex_;
};

}  // namespace imu
