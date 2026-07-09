#include "DatabaseManager.h"

#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>
#include <QtConcurrent>
#include <QVariant>
#include <QVariantMap>
#include <utility>

namespace {

constexpr int kLiveSensorSampleLimit = 240;

QString makeConnectionName()
{
    return QStringLiteral("RideManagerQml_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

bool openDatabase(const DatabaseConfig &config, const QString &connectionName, QString *errorMessage)
{
    if (!QSqlDatabase::isDriverAvailable(config.driverName)) {
        *errorMessage = QStringLiteral("Qt SQL driver %1 is not available. Available drivers: %2")
                            .arg(config.driverName, QSqlDatabase::drivers().join(QStringLiteral(", ")));
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(config.driverName, connectionName);
    db.setDatabaseName(config.databaseName);
    if (!config.connectOptions.isEmpty()) {
        db.setConnectOptions(config.connectOptions);
    }

    if (config.driverName != QStringLiteral("QSQLITE")) {
        db.setHostName(config.hostName);
        db.setPort(config.port);
        db.setUserName(config.userName);
        db.setPassword(config.password);
    }

    if (!db.open()) {
        *errorMessage = QStringLiteral("%1 connect %2:%3/%4 failed: %5")
                            .arg(config.driverName,
                                 config.hostName,
                                 QString::number(config.port),
                                 config.databaseName,
                                 db.lastError().text());
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    return true;
}

QString formatDateTime(const QVariant &value)
{
    const QDateTime dateTime = value.toDateTime();
    if (dateTime.isValid()) {
        return dateTime.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    }
    return value.toString();
}

void appendCameraFindingRows(QSqlQuery *query, CameraFindingQueryResult *result)
{
    while (query->next()) {
        CameraFindingItem item;
        item.cameraId = query->value(0).toString();
        item.label = query->value(1).toString();
        item.confidence = query->value(2).toDouble();
        item.observedAt = query->value(3).toDateTime();
        item.boxX = query->value(4).toDouble();
        item.boxY = query->value(5).toDouble();
        item.boxWidth = query->value(6).toDouble();
        item.boxHeight = query->value(7).toDouble();
        result->items.append(item);
    }
}

void closeDatabase(const QString &connectionName)
{
    QSqlDatabase db = QSqlDatabase::database(connectionName, false);
    if (db.isValid()) {
        db.close();
    }
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

class SqlConnectionGuard
{
public:
    explicit SqlConnectionGuard(QString connectionName)
        : m_connectionName(std::move(connectionName))
    {
    }

    ~SqlConnectionGuard()
    {
        closeDatabase(m_connectionName);
    }

private:
    QString m_connectionName;
};

SafetyDecisionQueryResult fetchSafetyDecisions(const DatabaseConfig &config, const QString &suffix)
{
    SafetyDecisionQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "select id, risk_level, decided_at "
        "from safety_decisions "
        "order by decided_at desc %1").arg(suffix);

    if (!query.exec(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    while (query.next()) {
        SafetyDecisionItem item;
        item.decisionId = query.value(0).toString();
        item.riskLevel = query.value(1).toString();
        item.decidedAt = query.value(2).toDateTime();
        result.items.append(item);
    }

    result.ok = true;
    return result;
}

SafetyDecisionQueryResult fetchLatestSafetyDecision(const DatabaseConfig &config)
{
    return fetchSafetyDecisions(config, QStringLiteral("limit 1"));
}

SafetyDecisionQueryResult fetchAllSafetyDecisions(const DatabaseConfig &config)
{
    return fetchSafetyDecisions(config, QString());
}

CameraFindingQueryResult fetchCameraFindings(const DatabaseConfig &config, const QString &decisionId)
{
    CameraFindingQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "select camera_id, label, confidence, observed_at, box_x, box_y, box_width, box_height "
        "from camera_findings "
        "where safety_decision_id = :decision_id "
        "order by observed_at"));
    query.bindValue(QStringLiteral(":decision_id"), decisionId);

    if (!query.exec()) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    appendCameraFindingRows(&query, &result);

    result.ok = true;
    return result;
}

CameraFindingQueryResult fetchAllCameraFindings(const DatabaseConfig &config)
{
    CameraFindingQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "select camera_id, label, confidence, observed_at, box_x, box_y, box_width, box_height "
        "from camera_findings "
        "order by observed_at");

    if (!query.exec(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    appendCameraFindingRows(&query, &result);
    result.ok = true;
    return result;
}

CameraFindingQueryResult fetchLatestCameraFindings(const DatabaseConfig &config)
{
    CameraFindingQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "select cf.camera_id, cf.label, cf.confidence, cf.observed_at, "
        "cf.box_x, cf.box_y, cf.box_width, cf.box_height "
        "from camera_findings cf "
        "join ("
        "select camera_id, max(observed_at) as observed_at "
        "from camera_findings "
        "group by camera_id"
        ") latest on latest.camera_id = cf.camera_id and latest.observed_at = cf.observed_at "
        "order by cf.observed_at, cf.camera_id");

    if (!query.exec(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    appendCameraFindingRows(&query, &result);
    result.ok = true;
    return result;
}

SensorReadingQueryResult fetchSensorReadings(const DatabaseConfig &config,
                                             const QString &sensorName,
                                             const QString &metric)
{
    SensorReadingQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    const QString sensorAlias = sensorName.compare(QStringLiteral("IMU"), Qt::CaseInsensitive) == 0
                                    ? QStringLiteral("GYRO")
                                    : (sensorName.compare(QStringLiteral("GYRO"), Qt::CaseInsensitive) == 0
                                           ? QStringLiteral("IMU")
                                           : sensorName);
    query.prepare(QStringLiteral(
        "select observed_at, sensor_name, metric, value, unit "
        "from ("
        "select ss.observed_at, ss.sensor_name, sr.metric, sr.value, sr.unit "
        "from sensor_readings sr "
        "join sensor_snapshots ss on ss.id = sr.sensor_snapshot_id "
        "where (lower(ss.sensor_name) = lower(:sensor_name) or lower(ss.sensor_name) = lower(:sensor_alias)) "
        "and lower(sr.metric) = lower(:metric) "
        "order by ss.observed_at desc "
        "limit :sample_limit"
        ") recent_readings "
        "order by observed_at"));
    query.bindValue(QStringLiteral(":sensor_name"), sensorName);
    query.bindValue(QStringLiteral(":sensor_alias"), sensorAlias);
    query.bindValue(QStringLiteral(":metric"), metric);
    query.bindValue(QStringLiteral(":sample_limit"), kLiveSensorSampleLimit);

    if (!query.exec()) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    while (query.next()) {
        SensorReadingItem item;
        item.sensorName = query.value(1).toString();
        item.observedAt = query.value(0).toDateTime();
        item.metric = query.value(2).toString();
        item.value = query.value(3).toDouble();
        item.unit = query.value(4).toString();
        result.items.append(item);
    }

    result.ok = true;
    return result;
}

TableRowsQueryResult fetchAllSensorReadingsRows(const DatabaseConfig &config)
{
    TableRowsQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "select ss.sensor_name, sr.metric, sr.value, sr.unit, ss.observed_at "
        "from sensor_readings sr "
        "join sensor_snapshots ss on ss.id = sr.sensor_snapshot_id "
        "order by ss.observed_at desc, ss.sensor_name, sr.metric");

    if (!query.exec(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        const QString metric = query.value(1).toString();
        row.insert(QStringLiteral("sensorName"), query.value(0).toString());
        row.insert(QStringLiteral("metric"), metric);
        row.insert(QStringLiteral("metricDisplay"), metric);
        row.insert(QStringLiteral("value"), query.value(2).toDouble());
        row.insert(QStringLiteral("unit"), query.value(3).toString());
        row.insert(QStringLiteral("observedAt"), formatDateTime(query.value(4)));
        result.rows.append(row);
    }

    result.ok = true;
    return result;
}

TableRowsQueryResult fetchAllActuatorCommands(const DatabaseConfig &config)
{
    TableRowsQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "select actuator_name, command_type, status, requested_at "
        "from actuator_commands "
        "order by requested_at desc");

    if (!query.exec(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("actuatorName"), query.value(0).toString());
        row.insert(QStringLiteral("commandType"), query.value(1).toString());
        row.insert(QStringLiteral("status"), query.value(2).toString());
        row.insert(QStringLiteral("requestedAt"), formatDateTime(query.value(3)));
        result.rows.append(row);
    }

    result.ok = true;
    return result;
}

TableRowsQueryResult fetchAllSystemEvents(const DatabaseConfig &config)
{
    TableRowsQueryResult result;
    const QString connectionName = makeConnectionName();

    if (!openDatabase(config, connectionName, &result.errorMessage)) {
        return result;
    }

    SqlConnectionGuard guard(connectionName);
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "select source, level, message, occurred_at "
        "from system_events "
        "order by case "
        "when lower(message) like '%start%' or message like '%开始%' or message like '%启动%' then 1 "
        "else 0 end, occurred_at desc, id desc");

    if (!query.exec(sql)) {
        result.errorMessage = query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("source"), query.value(0).toString());
        row.insert(QStringLiteral("level"), query.value(1).toString());
        row.insert(QStringLiteral("message"), query.value(2).toString());
        row.insert(QStringLiteral("occurredAt"), formatDateTime(query.value(3)));
        result.rows.append(row);
    }

    result.ok = true;
    return result;
}

} // namespace

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

void DatabaseManager::setConfig(const DatabaseConfig &config)
{
    QMutexLocker locker(&m_mutex);
    m_config = config;
}

DatabaseConfig DatabaseManager::config() const
{
    return copyConfig();
}

QFuture<SafetyDecisionQueryResult> DatabaseManager::fetchLatestSafetyDecisionAsync() const
{
    return QtConcurrent::run([config = copyConfig()]() {
        return fetchLatestSafetyDecision(config);
    });
}

QFuture<SafetyDecisionQueryResult> DatabaseManager::fetchAllSafetyDecisionsAsync() const
{
    return QtConcurrent::run([config = copyConfig()]() {
        return fetchAllSafetyDecisions(config);
    });
}

QFuture<CameraFindingQueryResult> DatabaseManager::fetchCameraFindingsAsync(const QString &decisionId) const
{
    return QtConcurrent::run([config = copyConfig(), decisionId]() {
        return fetchCameraFindings(config, decisionId);
    });
}

QFuture<CameraFindingQueryResult> DatabaseManager::fetchAllCameraFindingsAsync() const
{
    return QtConcurrent::run([config = copyConfig()]() {
        return fetchAllCameraFindings(config);
    });
}

QFuture<CameraFindingQueryResult> DatabaseManager::fetchLatestCameraFindingsAsync() const
{
    return QtConcurrent::run([config = copyConfig()]() {
        return fetchLatestCameraFindings(config);
    });
}

QFuture<SensorReadingQueryResult> DatabaseManager::fetchSensorReadingsAsync(const QString &sensorName,
                                                                            const QString &metric) const
{
    return QtConcurrent::run([config = copyConfig(), sensorName, metric]() {
        return fetchSensorReadings(config, sensorName, metric);
    });
}

QFuture<TableRowsQueryResult> DatabaseManager::fetchAllSensorReadingsRowsAsync() const
{
    return QtConcurrent::run([config = copyConfig()]() {
        return fetchAllSensorReadingsRows(config);
    });
}

QFuture<TableRowsQueryResult> DatabaseManager::fetchAllActuatorCommandsAsync() const
{
    return QtConcurrent::run([config = copyConfig()]() {
        return fetchAllActuatorCommands(config);
    });
}

QFuture<TableRowsQueryResult> DatabaseManager::fetchAllSystemEventsAsync() const
{
    return QtConcurrent::run([config = copyConfig()]() {
        return fetchAllSystemEvents(config);
    });
}

DatabaseConfig DatabaseManager::copyConfig() const
{
    QMutexLocker locker(&m_mutex);
    return m_config;
}
