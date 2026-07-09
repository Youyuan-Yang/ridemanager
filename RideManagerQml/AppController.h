#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "CameraFindingModel.h"
#include "DatabaseManager.h"
#include "SafetyDecisionModel.h"
#include "SensorCatalog.h"
#include "SensorReadingModel.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SafetyDecisionModel *safetyDecisionModel READ safetyDecisionModel CONSTANT)
    Q_PROPERTY(CameraFindingModel *cameraFindingModel READ cameraFindingModel CONSTANT)
    Q_PROPERTY(CameraFindingModel *liveCameraFindingModel READ liveCameraFindingModel CONSTANT)
    Q_PROPERTY(CameraFindingModel *currentDecisionCameraFindingModel READ currentDecisionCameraFindingModel CONSTANT)
    Q_PROPERTY(SensorReadingModel *sensorReadingModel READ sensorReadingModel CONSTANT)
    Q_PROPERTY(QString databaseStatus READ databaseStatus NOTIFY databaseStatusChanged)
    Q_PROPERTY(QString databaseEndpoint READ databaseEndpoint NOTIFY databaseEndpointChanged)
    Q_PROPERTY(QString lastRefreshTime READ lastRefreshTime NOTIFY lastRefreshTimeChanged)
    Q_PROPERTY(QString currentRiskLevel READ currentRiskLevel NOTIFY currentRiskLevelChanged)
    Q_PROPERTY(QString selectedDecisionId READ selectedDecisionId NOTIFY selectedDecisionIdChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList sensorNames READ sensorNames CONSTANT)
    Q_PROPERTY(QVariantList sensorReadingRows READ sensorReadingRows NOTIFY sensorReadingRowsChanged)
    Q_PROPERTY(QVariantList actuatorCommandRows READ actuatorCommandRows NOTIFY actuatorCommandRowsChanged)
    Q_PROPERTY(QVariantList systemEventRows READ systemEventRows NOTIFY systemEventRowsChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    SafetyDecisionModel *safetyDecisionModel();
    CameraFindingModel *cameraFindingModel();
    CameraFindingModel *liveCameraFindingModel();
    CameraFindingModel *currentDecisionCameraFindingModel();
    SensorReadingModel *sensorReadingModel();

    QString databaseStatus() const;
    QString databaseEndpoint() const;
    QString lastRefreshTime() const;
    QString currentRiskLevel() const;
    QString selectedDecisionId() const;

    int currentPage() const;
    void setCurrentPage(int page);

    bool busy() const;
    QVariantList sensorNames() const;
    QVariantList sensorReadingRows() const;
    QVariantList actuatorCommandRows() const;
    QVariantList systemEventRows() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectDecision(const QString &decisionId);
    Q_INVOKABLE void loadSensorMetric(const QString &sensorName, const QString &metric);
    Q_INVOKABLE QVariantList metricNamesForSensor(const QString &sensorName) const;
    Q_INVOKABLE QString sensorDescription(const QString &sensorName) const;
    Q_INVOKABLE QString metricDisplayName(const QString &metricName) const;
    Q_INVOKABLE QString sensorAccentColor(const QString &sensorName) const;

signals:
    void databaseStatusChanged();
    void databaseEndpointChanged();
    void lastRefreshTimeChanged();
    void currentRiskLevelChanged();
    void selectedDecisionIdChanged();
    void currentPageChanged();
    void busyChanged();
    void sensorReadingRowsChanged();
    void actuatorCommandRowsChanged();
    void systemEventRowsChanged();

private:
    void configureRideManagerDatabase();
    void refreshInternal(bool interactive);
    void refreshRealtimeDecision();
    void refreshRealtimeCameraFindings();
    void refreshRealtimeLiveCameraFindings();
    void refreshRealtimeCurrentDecisionCameraFindings();
    void refreshRealtimeSensorRows();
    void refreshRealtimeActuatorCommands();
    void refreshRealtimeEvents();
    void refreshRealtimeSensorReadings();
    void refreshAuxiliaryTables();
    void setDatabaseStatus(const QString &status);
    void setLastRefreshToNow();
    void setCurrentRiskLevel(const QString &riskLevel);
    void setSelectedDecisionId(const QString &decisionId);
    void setBusy(bool busy);
    void beginAsyncOperation();
    void endAsyncOperation();

    DatabaseManager m_database;
    SafetyDecisionModel m_safetyDecisionModel;
    CameraFindingModel m_cameraFindingModel;
    CameraFindingModel m_liveCameraFindingModel;
    CameraFindingModel m_currentDecisionCameraFindingModel;
    SensorReadingModel m_sensorReadingModel;

    bool m_databaseReady = false;
    QString m_databaseStatus = QStringLiteral("正在读取 RideManager 数据库配置");
    QString m_databaseEndpoint;
    QString m_lastRefreshTime = QStringLiteral("--");
    QString m_currentRiskLevel = QStringLiteral("Normal");
    QString m_selectedDecisionId;
    int m_currentPage = 0;
    bool m_busy = false;
    int m_pendingOperations = 0;
    int m_refreshGeneration = 0;
    int m_cameraGeneration = 0;
    int m_liveCameraGeneration = 0;
    int m_currentDecisionCameraGeneration = 0;
    int m_realtimeCurrentDecisionCameraGeneration = 0;
    int m_sensorGeneration = 0;
    int m_auxiliaryGeneration = 0;
    int m_realtimeDecisionGeneration = 0;
    int m_realtimeActuatorGeneration = 0;
    int m_realtimeEventGeneration = 0;
    int m_realtimeSensorGeneration = 0;
    int m_realtimeSensorRowsGeneration = 0;
    QString m_lastSensorName = SensorCatalog::defaultSensorName();
    QString m_lastMetric = SensorCatalog::defaultMetricName();

    QTimer m_refreshTimer;
    QTimer m_realtimeTimer;
    bool m_backgroundRefreshInFlight = false;
    bool m_realtimeCameraInFlight = false;
    bool m_realtimeLiveCameraInFlight = false;
    bool m_realtimeCurrentDecisionCameraInFlight = false;
    bool m_realtimeActuatorInFlight = false;
    bool m_realtimeDecisionInFlight = false;
    bool m_realtimeEventInFlight = false;
    bool m_realtimeSensorInFlight = false;
    bool m_realtimeSensorRowsInFlight = false;
    QVariantList m_sensorRows;
    QVariantList m_actuatorRows;
    QVariantList m_systemRows;
};

#endif // APPCONTROLLER_H
