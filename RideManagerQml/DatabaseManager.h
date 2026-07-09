#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QDateTime>
#include <QFuture>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariantList>

struct SafetyDecisionItem
{
    QString decisionId;
    QString riskLevel;
    QDateTime decidedAt;
};

struct CameraFindingItem
{
    QString cameraId;
    QString label;
    double confidence = 0.0;
    QDateTime observedAt;
    double boxX = 0.0;
    double boxY = 0.0;
    double boxWidth = 0.0;
    double boxHeight = 0.0;
};

struct SensorReadingItem
{
    QString sensorName;
    QString metric;
    double value = 0.0;
    QString unit;
    QDateTime observedAt;
};

struct DatabaseConfig
{
    QString driverName = QStringLiteral("QPSQL");
    QString hostName = QStringLiteral("127.0.0.1");
    int port = 5432;
    QString databaseName = QStringLiteral("ridemanager");
    QString userName = QStringLiteral("ridemanager");
    QString password = QStringLiteral("ridemanager");
    QString connectOptions;
};

struct SafetyDecisionQueryResult
{
    bool ok = false;
    QString errorMessage;
    QList<SafetyDecisionItem> items;
};

struct CameraFindingQueryResult
{
    bool ok = false;
    QString errorMessage;
    QList<CameraFindingItem> items;
};

struct SensorReadingQueryResult
{
    bool ok = false;
    QString errorMessage;
    QList<SensorReadingItem> items;
};

struct TableRowsQueryResult
{
    bool ok = false;
    QString errorMessage;
    QVariantList rows;
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);

    void setConfig(const DatabaseConfig &config);
    DatabaseConfig config() const;

    QFuture<SafetyDecisionQueryResult> fetchLatestSafetyDecisionAsync() const;
    QFuture<SafetyDecisionQueryResult> fetchAllSafetyDecisionsAsync() const;
    QFuture<CameraFindingQueryResult> fetchCameraFindingsAsync(const QString &decisionId) const;
    QFuture<CameraFindingQueryResult> fetchAllCameraFindingsAsync() const;
    QFuture<CameraFindingQueryResult> fetchLatestCameraFindingsAsync() const;
    QFuture<SensorReadingQueryResult> fetchSensorReadingsAsync(const QString &sensorName,
                                                               const QString &metric) const;
    QFuture<TableRowsQueryResult> fetchAllSensorReadingsRowsAsync() const;
    QFuture<TableRowsQueryResult> fetchAllActuatorCommandsAsync() const;
    QFuture<TableRowsQueryResult> fetchAllSystemEventsAsync() const;

private:
    DatabaseConfig copyConfig() const;

    mutable QMutex m_mutex;
    DatabaseConfig m_config;
};

#endif // DATABASEMANAGER_H
