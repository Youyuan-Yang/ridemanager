#include "database/PostgreSQLManager.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <libpq-fe.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "common/Logger.h"
#include "common/TimeUtils.h"
#include "common/Uuid.h"

namespace imu {

namespace {

constexpr const char* kInsertSnapshotStatement = "insert_imu_sensor_snapshot";
constexpr const char* kInsertReadingStatement = "insert_imu_sensor_reading";
constexpr const char* kInsertSafetyDecisionStatement = "insert_imu_safety_decision";
constexpr const char* kZeroUuid = "00000000-0000-0000-0000-000000000001";

using PGResultPtr = std::unique_ptr<PGresult, decltype(&PQclear)>;

void checkResult(PGresult* result, ExecStatusType expectedStatus, const std::string& context) {
    if (result == nullptr) {
        throw std::runtime_error(context + ": null PostgreSQL result");
    }

    if (PQresultStatus(result) != expectedStatus) {
        throw std::runtime_error(context + ": " + std::string(PQresultErrorMessage(result)));
    }
}

std::string formatDouble(double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error("non-finite value cannot be written to PostgreSQL");
    }

    std::ostringstream oss;
    oss << std::setprecision(15) << value;
    return oss.str();
}

bool shouldWriteNullDeviceId(const std::string& deviceId) {
    return deviceId.empty() || deviceId == kZeroUuid;
}

std::string escapeJsonString(const std::string& value) {
    std::ostringstream oss;
    for (const char ch : value) {
        switch (ch) {
            case '"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\b':
                oss << "\\b";
                break;
            case '\f':
                oss << "\\f";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << ch;
                break;
        }
    }
    return oss.str();
}

std::string buildSafetyDecisionPayloadJson(
    const std::string& sensorName,
    const std::vector<ProcessedIMUData>& batch) {
    std::ostringstream oss;
    oss << '{'
        << "\"source\":\"imu_service\","
        << "\"sensor_name\":\"" << escapeJsonString(sensorName) << "\","
        << "\"batch_size\":" << batch.size() << ','
        << "\"note\":\"auto-created safety decision for standalone IMU snapshot\""
        << '}';
    return oss.str();
}

std::string buildValuesJson(const ProcessedIMUData& data) {
    std::ostringstream oss;
    oss << '{'
        << "\"acc_x\":" << formatDouble(data.accelerationG.x) << ','
        << "\"acc_y\":" << formatDouble(data.accelerationG.y) << ','
        << "\"acc_z\":" << formatDouble(data.accelerationG.z) << ','
        << "\"gyro_x\":" << formatDouble(data.gyroscopeDps.x) << ','
        << "\"gyro_y\":" << formatDouble(data.gyroscopeDps.y) << ','
        << "\"gyro_z\":" << formatDouble(data.gyroscopeDps.z) << ','
        << "\"temperature\":" << formatDouble(data.temperatureC) << ','
        << "\"roll\":" << formatDouble(data.eulerDeg.roll) << ','
        << "\"pitch\":" << formatDouble(data.eulerDeg.pitch) << ','
        << "\"yaw\":" << formatDouble(data.eulerDeg.yaw)
        << '}';
    return oss.str();
}

struct Reading {
    const char* metric;
    double value;
    const char* unit;
};

std::array<Reading, 9> buildReadings(const ProcessedIMUData& data) {
    return {
        Reading{"acc_x", data.accelerationG.x, "g"},
        Reading{"acc_y", data.accelerationG.y, "g"},
        Reading{"acc_z", data.accelerationG.z, "g"},
        Reading{"gyro_x", data.gyroscopeDps.x, "deg/s"},
        Reading{"gyro_y", data.gyroscopeDps.y, "deg/s"},
        Reading{"gyro_z", data.gyroscopeDps.z, "deg/s"},
        Reading{"roll", data.eulerDeg.roll, "degree"},
        Reading{"pitch", data.eulerDeg.pitch, "degree"},
        Reading{"yaw", data.eulerDeg.yaw, "degree"},
    };
}

}  // namespace

PostgreSQLManager::PostgreSQLManager(std::string conninfo)
    : conninfo_(std::move(conninfo)) {}

PostgreSQLManager::~PostgreSQLManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    disconnectLocked();
}

void PostgreSQLManager::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    connectLocked();
}

void PostgreSQLManager::insertBatch(
    const std::optional<std::string>& safetyDecisionId,
    const std::string& deviceId,
    const std::string& sensorName,
    const std::vector<ProcessedIMUData>& batch) {
    if (batch.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    for (int attempt = 0; attempt < 2; ++attempt) {
        try {
            ensureConnectedLocked();
            execSimpleLocked("BEGIN");

            const bool createdSafetyDecision = !safetyDecisionId.has_value();
            const std::string effectiveSafetyDecisionId =
                safetyDecisionId.has_value() ? safetyDecisionId.value() : generateUuidV4();
            if (createdSafetyDecision) {
                insertSafetyDecisionLocked(effectiveSafetyDecisionId, sensorName, batch);
            }

            for (const auto& data : batch) {
                const std::string snapshotId = generateUuidV4();
                insertSnapshotLocked(snapshotId, effectiveSafetyDecisionId, deviceId, sensorName, data);

                for (const auto& reading : buildReadings(data)) {
                    insertReadingLocked(
                        generateUuidV4(),
                        snapshotId,
                        reading.metric,
                        reading.value,
                        reading.unit);
                }
            }

            execSimpleLocked("COMMIT");
            Logger::info("Database insert success, batch=" + std::to_string(batch.size()));
            return;
        } catch (...) {
            try {
                if (conn_ != nullptr) {
                    execSimpleLocked("ROLLBACK");
                }
            } catch (...) {
            }

            if (attempt == 0 && connectionBadLocked()) {
                Logger::warn("PostgreSQL connection lost, reconnecting");
                disconnectLocked();
                continue;
            }

            throw;
        }
    }
}

void PostgreSQLManager::connectLocked() {
    disconnectLocked();

    conn_ = PQconnectdb(conninfo_.c_str());
    if (conn_ == nullptr) {
        throw std::runtime_error("PQconnectdb returned null");
    }

    if (PQstatus(conn_) != CONNECTION_OK) {
        const std::string error = PQerrorMessage(conn_);
        disconnectLocked();
        throw std::runtime_error("failed to connect PostgreSQL: " + error);
    }

    prepareStatementsLocked();
    Logger::info("PostgreSQL connected");
}

void PostgreSQLManager::disconnectLocked() noexcept {
    if (conn_ != nullptr) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
    prepared_ = false;
}

void PostgreSQLManager::ensureConnectedLocked() {
    if (conn_ == nullptr || PQstatus(conn_) != CONNECTION_OK || !prepared_) {
        connectLocked();
    }
}

void PostgreSQLManager::prepareStatementsLocked() {
    const char* snapshotSql =
        "INSERT INTO sensor_snapshots "
        "(id, safety_decision_id, device_id, sensor_name, observed_at, values_json) "
        "VALUES ($1::uuid, $2::uuid, $3::uuid, $4::varchar, $5::timestamptz, $6::jsonb)";

    PGResultPtr snapshotResult(
        PQprepare(conn_, kInsertSnapshotStatement, snapshotSql, 6, nullptr),
        &PQclear);
    checkResult(snapshotResult.get(), PGRES_COMMAND_OK, "prepare sensor_snapshots");

    const char* safetyDecisionSql =
        "INSERT INTO safety_decisions "
        "(id, run_session_id, risk_level, decided_at, payload_json, created_at) "
        "VALUES ($1::uuid, NULL, $2::varchar, $3::timestamptz, $4::jsonb, $3::timestamptz)";

    PGResultPtr safetyDecisionResult(
        PQprepare(conn_, kInsertSafetyDecisionStatement, safetyDecisionSql, 4, nullptr),
        &PQclear);
    checkResult(safetyDecisionResult.get(), PGRES_COMMAND_OK, "prepare safety_decisions");

    const char* readingSql =
        "INSERT INTO sensor_readings "
        "(id, sensor_snapshot_id, metric, value, unit) "
        "VALUES ($1::uuid, $2::uuid, $3::varchar, $4::double precision, $5::varchar)";

    PGResultPtr readingResult(
        PQprepare(conn_, kInsertReadingStatement, readingSql, 5, nullptr),
        &PQclear);
    checkResult(readingResult.get(), PGRES_COMMAND_OK, "prepare sensor_readings");

    prepared_ = true;
}

void PostgreSQLManager::execSimpleLocked(const std::string& sql) {
    PGResultPtr result(PQexec(conn_, sql.c_str()), &PQclear);
    checkResult(result.get(), PGRES_COMMAND_OK, sql);
}

void PostgreSQLManager::insertSafetyDecisionLocked(
    const std::string& safetyDecisionId,
    const std::string& sensorName,
    const std::vector<ProcessedIMUData>& batch) {
    const std::string decidedAt = formatTimestampUtc(batch.back().observedAt);
    const std::string payloadJson = buildSafetyDecisionPayloadJson(sensorName, batch);
    const std::string riskLevel = "Normal";
    const std::array<const char*, 4> values{
        safetyDecisionId.c_str(),
        riskLevel.c_str(),
        decidedAt.c_str(),
        payloadJson.c_str(),
    };

    PGResultPtr result(
        PQexecPrepared(conn_, kInsertSafetyDecisionStatement, 4, values.data(), nullptr, nullptr, 0),
        &PQclear);
    checkResult(result.get(), PGRES_COMMAND_OK, "insert safety_decisions");
}

void PostgreSQLManager::insertSnapshotLocked(
    const std::string& snapshotId,
    const std::string& safetyDecisionId,
    const std::string& deviceId,
    const std::string& sensorName,
    const ProcessedIMUData& data) {
    const std::string observedAt = formatTimestampUtc(data.observedAt);
    const std::string valuesJson = buildValuesJson(data);
    const char* deviceIdValue = shouldWriteNullDeviceId(deviceId) ? nullptr : deviceId.c_str();

    const std::array<const char*, 6> values{
        snapshotId.c_str(),
        safetyDecisionId.c_str(),
        deviceIdValue,
        sensorName.c_str(),
        observedAt.c_str(),
        valuesJson.c_str(),
    };

    PGResultPtr result(
        PQexecPrepared(conn_, kInsertSnapshotStatement, 6, values.data(), nullptr, nullptr, 0),
        &PQclear);
    checkResult(result.get(), PGRES_COMMAND_OK, "insert sensor_snapshots");
}

void PostgreSQLManager::insertReadingLocked(
    const std::string& readingId,
    const std::string& snapshotId,
    const std::string& metric,
    double value,
    const std::string& unit) {
    const std::string valueText = formatDouble(value);
    const std::array<const char*, 5> values{
        readingId.c_str(),
        snapshotId.c_str(),
        metric.c_str(),
        valueText.c_str(),
        unit.c_str(),
    };

    PGResultPtr result(
        PQexecPrepared(conn_, kInsertReadingStatement, 5, values.data(), nullptr, nullptr, 0),
        &PQclear);
    checkResult(result.get(), PGRES_COMMAND_OK, "insert sensor_readings");
}

bool PostgreSQLManager::connectionBadLocked() const {
    return conn_ == nullptr || PQstatus(conn_) == CONNECTION_BAD;
}

}  // namespace imu
