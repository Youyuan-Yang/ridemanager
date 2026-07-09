#include "AppController.h"

#include <QDateTime>
#include <QDebug>
#include <QFutureWatcher>
#include <QVariantMap>

#include "RideManagerConfig.h"

namespace {

constexpr int kFullRefreshIntervalMs = 1000;
constexpr int kRealtimePollingIntervalMs = 100;

bool sameRows(const QVariantList &left, const QVariantList &right)
{
    if (left.count() != right.count()) {
        return false;
    }
    for (int i = 0; i < left.count(); ++i) {
        if (left.at(i).toMap() != right.at(i).toMap()) {
            return false;
        }
    }
    return true;
}

QString riskLevelFromEventLevel(const QString &level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized == QStringLiteral("error") || normalized == QStringLiteral("danger")) {
        return QStringLiteral("Danger");
    }
    if (normalized == QStringLiteral("warning") || normalized == QStringLiteral("warn")) {
        return QStringLiteral("Warning");
    }
    return {};
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    configureRideManagerDatabase();
}

SafetyDecisionModel *AppController::safetyDecisionModel()
{
    return &m_safetyDecisionModel;
}

CameraFindingModel *AppController::cameraFindingModel()
{
    return &m_cameraFindingModel;
}

CameraFindingModel *AppController::liveCameraFindingModel()
{
    return &m_liveCameraFindingModel;
}

CameraFindingModel *AppController::currentDecisionCameraFindingModel()
{
    return &m_currentDecisionCameraFindingModel;
}

SensorReadingModel *AppController::sensorReadingModel()
{
    return &m_sensorReadingModel;
}

QString AppController::databaseStatus() const
{
    return m_databaseStatus;
}

QString AppController::databaseEndpoint() const
{
    return m_databaseEndpoint;
}

QString AppController::lastRefreshTime() const
{
    return m_lastRefreshTime;
}

QString AppController::currentRiskLevel() const
{
    return m_currentRiskLevel;
}

QString AppController::selectedDecisionId() const
{
    return m_selectedDecisionId;
}

int AppController::currentPage() const
{
    return m_currentPage;
}

void AppController::setCurrentPage(int page)
{
    if (m_currentPage == page) {
        return;
    }

    m_currentPage = page;
    emit currentPageChanged();

    if (page == 2) {
        loadSensorMetric(m_lastSensorName, m_lastMetric);
    }
}

bool AppController::busy() const
{
    return m_busy;
}

QVariantList AppController::sensorNames() const
{
    return SensorCatalog::sensorNames();
}

QVariantList AppController::sensorReadingRows() const
{
    return m_sensorRows;
}

QVariantList AppController::actuatorCommandRows() const
{
    return m_actuatorRows;
}

QVariantList AppController::systemEventRows() const
{
    return m_systemRows;
}

void AppController::refresh()
{
    refreshInternal(true);
}

void AppController::refreshInternal(bool interactive)
{
    ++m_refreshGeneration;
    const int generation = m_refreshGeneration;

    if (!m_databaseReady) {
        setDatabaseStatus(QStringLiteral("RideManager PostgreSQL 配置尚未就绪"));
        return;
    }

    if (m_lastRefreshTime == QStringLiteral("--")
        && !m_databaseStatus.startsWith(QStringLiteral("RideManager PostgreSQL 已连接"))) {
        setDatabaseStatus(QStringLiteral("正在异步读取 RideManager PostgreSQL..."));
    }

    if (interactive) {
        beginAsyncOperation();
    } else {
        m_backgroundRefreshInFlight = true;
    }

    auto *watcher = new QFutureWatcher<SafetyDecisionQueryResult>(this);
    connect(watcher, &QFutureWatcher<SafetyDecisionQueryResult>::finished, this, [this, watcher, generation, interactive]() {
        const SafetyDecisionQueryResult result = watcher->result();
        watcher->deleteLater();
        if (interactive) {
            endAsyncOperation();
        } else {
            m_backgroundRefreshInFlight = false;
        }

        if (generation != m_refreshGeneration) {
            return;
        }

        if (!result.ok) {
            m_safetyDecisionModel.clear();
            m_cameraFindingModel.clear();
            m_liveCameraFindingModel.clear();
            m_currentDecisionCameraFindingModel.clear();
            m_sensorReadingModel.clear();
            setDatabaseStatus(QStringLiteral("数据库错误: %1").arg(result.errorMessage));
            return;
        }

        m_safetyDecisionModel.setDecisions(result.items);
        setDatabaseStatus(
            QStringLiteral("RideManager PostgreSQL 已连接，读取 %1 条安全决策")
                .arg(result.items.count()));
        setLastRefreshToNow();

        if (!result.items.isEmpty()) {
            const QString decisionToLoad =
                m_selectedDecisionId.isEmpty() ? result.items.first().decisionId : m_selectedDecisionId;
            selectDecision(decisionToLoad);
        } else {
            setSelectedDecisionId({});
            setCurrentRiskLevel(QStringLiteral("Normal"));
            m_cameraFindingModel.clear();
            m_liveCameraFindingModel.clear();
            m_currentDecisionCameraFindingModel.clear();
        }

        loadSensorMetric(m_lastSensorName, m_lastMetric);
        refreshAuxiliaryTables();
    });
    watcher->setFuture(m_database.fetchAllSafetyDecisionsAsync());
}

void AppController::selectDecision(const QString &decisionId)
{
    if (decisionId.isEmpty()) {
        return;
    }

    setSelectedDecisionId(decisionId);
    setCurrentRiskLevel(m_safetyDecisionModel.riskLevelForDecision(decisionId));

    ++m_currentDecisionCameraGeneration;
    const int generation = m_currentDecisionCameraGeneration;
    beginAsyncOperation();

    auto *watcher = new QFutureWatcher<CameraFindingQueryResult>(this);
    connect(watcher, &QFutureWatcher<CameraFindingQueryResult>::finished, this, [this, watcher, generation]() {
        const CameraFindingQueryResult result = watcher->result();
        watcher->deleteLater();
        endAsyncOperation();

        if (generation != m_currentDecisionCameraGeneration) {
            return;
        }

        if (!result.ok) {
            m_currentDecisionCameraFindingModel.clear();
            setDatabaseStatus(QStringLiteral("当前决策摄像头检测查询失败: %1").arg(result.errorMessage));
            return;
        }

        m_currentDecisionCameraFindingModel.setFindings(result.items);
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchCameraFindingsAsync(decisionId));
}

void AppController::loadSensorMetric(const QString &sensorName, const QString &metric)
{
    if (sensorName.isEmpty() || metric.isEmpty()) {
        return;
    }

    m_lastSensorName = sensorName;
    m_lastMetric = metric;
    ++m_realtimeSensorGeneration;

    if (!m_databaseReady) {
        return;
    }

    ++m_sensorGeneration;
    const int generation = m_sensorGeneration;
    beginAsyncOperation();

    auto *watcher = new QFutureWatcher<SensorReadingQueryResult>(this);
    connect(watcher, &QFutureWatcher<SensorReadingQueryResult>::finished, this, [this, watcher, generation]() {
        const SensorReadingQueryResult result = watcher->result();
        watcher->deleteLater();
        endAsyncOperation();

        if (generation != m_sensorGeneration) {
            return;
        }

        if (!result.ok) {
            m_sensorReadingModel.clear();
            setDatabaseStatus(QStringLiteral("传感器查询失败: %1").arg(result.errorMessage));
            return;
        }

        m_sensorReadingModel.setReadings(result.items);
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchSensorReadingsAsync(sensorName, metric));
}

QVariantList AppController::metricNamesForSensor(const QString &sensorName) const
{
    return SensorCatalog::metricNamesForSensor(sensorName);
}

QString AppController::sensorDescription(const QString &sensorName) const
{
    return SensorCatalog::sensorDescription(sensorName);
}

QString AppController::metricDisplayName(const QString &metricName) const
{
    return SensorCatalog::metricDisplayName(metricName);
}

QString AppController::sensorAccentColor(const QString &sensorName) const
{
    return SensorCatalog::sensorAccentColor(sensorName);
}

void AppController::configureRideManagerDatabase()
{
    const RideManagerConfigResult result = RideManagerConfig::load();
    if (!result.ok) {
        setDatabaseStatus(result.errorMessage);
        return;
    }

    m_database.setConfig(result.database);
    m_databaseReady = true;
    m_databaseEndpoint = result.endpoint;
    emit databaseEndpointChanged();

    m_refreshTimer.setInterval(kFullRefreshIntervalMs);
    m_refreshTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (!m_busy && !m_backgroundRefreshInFlight) {
            refreshInternal(false);
            refreshRealtimeCameraFindings();
        }
    });
    m_refreshTimer.start();

    m_realtimeTimer.setInterval(kRealtimePollingIntervalMs);
    m_realtimeTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_realtimeTimer, &QTimer::timeout, this, [this]() {
        refreshRealtimeDecision();
        refreshRealtimeLiveCameraFindings();
        refreshRealtimeCurrentDecisionCameraFindings();
        refreshRealtimeSensorReadings();
    });
    m_realtimeTimer.start();

    setDatabaseStatus(QStringLiteral("已加载 RideManager 配置，准备连接 PostgreSQL"));
    refreshInternal(true);
}

void AppController::refreshRealtimeDecision()
{
    if (!m_databaseReady || m_realtimeDecisionInFlight) {
        return;
    }

    ++m_realtimeDecisionGeneration;
    const int generation = m_realtimeDecisionGeneration;
    m_realtimeDecisionInFlight = true;

    auto *watcher = new QFutureWatcher<SafetyDecisionQueryResult>(this);
    connect(watcher, &QFutureWatcher<SafetyDecisionQueryResult>::finished, this, [this, watcher, generation]() {
        const SafetyDecisionQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeDecisionInFlight = false;

        if (generation != m_realtimeDecisionGeneration || !result.ok || result.items.isEmpty()) {
            return;
        }

        const SafetyDecisionItem &latest = result.items.first();
        setCurrentRiskLevel(latest.riskLevel);
        setSelectedDecisionId(latest.decisionId);
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchLatestSafetyDecisionAsync());
}

void AppController::refreshRealtimeCameraFindings()
{
    if (!m_databaseReady || m_realtimeCameraInFlight) {
        return;
    }

    ++m_cameraGeneration;
    const int generation = m_cameraGeneration;
    m_realtimeCameraInFlight = true;

    auto *watcher = new QFutureWatcher<CameraFindingQueryResult>(this);
    connect(watcher, &QFutureWatcher<CameraFindingQueryResult>::finished, this, [this, watcher, generation]() {
        const CameraFindingQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeCameraInFlight = false;

        if (generation != m_cameraGeneration || !result.ok) {
            return;
        }

        m_cameraFindingModel.setFindings(result.items);
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchAllCameraFindingsAsync());
}

void AppController::refreshRealtimeLiveCameraFindings()
{
    if (!m_databaseReady || m_realtimeLiveCameraInFlight) {
        return;
    }

    ++m_liveCameraGeneration;
    const int generation = m_liveCameraGeneration;
    m_realtimeLiveCameraInFlight = true;

    auto *watcher = new QFutureWatcher<CameraFindingQueryResult>(this);
    connect(watcher, &QFutureWatcher<CameraFindingQueryResult>::finished, this, [this, watcher, generation]() {
        const CameraFindingQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeLiveCameraInFlight = false;

        if (generation != m_liveCameraGeneration || !result.ok) {
            return;
        }

        m_liveCameraFindingModel.setFindings(result.items);
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchLatestCameraFindingsAsync());
}

void AppController::refreshRealtimeCurrentDecisionCameraFindings()
{
    if (!m_databaseReady || m_realtimeCurrentDecisionCameraInFlight || m_selectedDecisionId.isEmpty()) {
        return;
    }

    ++m_realtimeCurrentDecisionCameraGeneration;
    const int generation = m_realtimeCurrentDecisionCameraGeneration;
    const QString decisionId = m_selectedDecisionId;
    m_realtimeCurrentDecisionCameraInFlight = true;

    auto *watcher = new QFutureWatcher<CameraFindingQueryResult>(this);
    connect(watcher, &QFutureWatcher<CameraFindingQueryResult>::finished, this, [this, watcher, generation, decisionId]() {
        const CameraFindingQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeCurrentDecisionCameraInFlight = false;

        if (generation != m_realtimeCurrentDecisionCameraGeneration
            || decisionId != m_selectedDecisionId
            || !result.ok) {
            return;
        }

        m_currentDecisionCameraFindingModel.setFindings(result.items);
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchCameraFindingsAsync(decisionId));
}

void AppController::refreshRealtimeSensorRows()
{
    if (!m_databaseReady || m_realtimeSensorRowsInFlight) {
        return;
    }

    ++m_realtimeSensorRowsGeneration;
    const int generation = m_realtimeSensorRowsGeneration;
    m_realtimeSensorRowsInFlight = true;

    auto *watcher = new QFutureWatcher<TableRowsQueryResult>(this);
    connect(watcher, &QFutureWatcher<TableRowsQueryResult>::finished, this, [this, watcher, generation]() {
        const TableRowsQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeSensorRowsInFlight = false;

        if (generation != m_realtimeSensorRowsGeneration || !result.ok) {
            return;
        }

        if (!sameRows(m_sensorRows, result.rows)) {
            m_sensorRows = result.rows;
            emit sensorReadingRowsChanged();
        }
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchAllSensorReadingsRowsAsync());
}

void AppController::refreshRealtimeActuatorCommands()
{
    if (!m_databaseReady || m_realtimeActuatorInFlight) {
        return;
    }

    ++m_realtimeActuatorGeneration;
    const int generation = m_realtimeActuatorGeneration;
    m_realtimeActuatorInFlight = true;

    auto *watcher = new QFutureWatcher<TableRowsQueryResult>(this);
    connect(watcher, &QFutureWatcher<TableRowsQueryResult>::finished, this, [this, watcher, generation]() {
        const TableRowsQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeActuatorInFlight = false;

        if (generation != m_realtimeActuatorGeneration || !result.ok) {
            return;
        }

        if (!sameRows(m_actuatorRows, result.rows)) {
            m_actuatorRows = result.rows;
            emit actuatorCommandRowsChanged();
        }
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchAllActuatorCommandsAsync());
}

void AppController::refreshRealtimeEvents()
{
    if (!m_databaseReady || m_realtimeEventInFlight) {
        return;
    }

    ++m_realtimeEventGeneration;
    const int generation = m_realtimeEventGeneration;
    m_realtimeEventInFlight = true;

    auto *watcher = new QFutureWatcher<TableRowsQueryResult>(this);
    connect(watcher, &QFutureWatcher<TableRowsQueryResult>::finished, this, [this, watcher, generation]() {
        const TableRowsQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeEventInFlight = false;

        if (generation != m_realtimeEventGeneration || !result.ok) {
            return;
        }

        if (!sameRows(m_systemRows, result.rows)) {
            m_systemRows = result.rows;
            emit systemEventRowsChanged();
        }

        if (!result.rows.isEmpty()) {
            const QString eventRisk = riskLevelFromEventLevel(result.rows.first().toMap().value(QStringLiteral("level")).toString());
            if (!eventRisk.isEmpty()) {
                setCurrentRiskLevel(eventRisk);
            }
        }
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchAllSystemEventsAsync());
}

void AppController::refreshRealtimeSensorReadings()
{
    if (!m_databaseReady
        || m_realtimeSensorInFlight
        || m_lastSensorName.isEmpty()
        || m_lastMetric.isEmpty()) {
        return;
    }

    ++m_realtimeSensorGeneration;
    const int generation = m_realtimeSensorGeneration;
    m_realtimeSensorInFlight = true;

    auto *watcher = new QFutureWatcher<SensorReadingQueryResult>(this);
    connect(watcher, &QFutureWatcher<SensorReadingQueryResult>::finished, this, [this, watcher, generation]() {
        const SensorReadingQueryResult result = watcher->result();
        watcher->deleteLater();
        m_realtimeSensorInFlight = false;

        if (generation != m_realtimeSensorGeneration || !result.ok) {
            return;
        }

        m_sensorReadingModel.setReadings(result.items);
        setLastRefreshToNow();
    });
    watcher->setFuture(m_database.fetchSensorReadingsAsync(m_lastSensorName, m_lastMetric));
}

void AppController::refreshAuxiliaryTables()
{
    ++m_auxiliaryGeneration;
    const int generation = m_auxiliaryGeneration;

    beginAsyncOperation();
    auto *actuatorWatcher = new QFutureWatcher<TableRowsQueryResult>(this);
    connect(actuatorWatcher, &QFutureWatcher<TableRowsQueryResult>::finished, this, [this, actuatorWatcher, generation]() {
        const TableRowsQueryResult result = actuatorWatcher->result();
        actuatorWatcher->deleteLater();
        endAsyncOperation();

        if (generation != m_auxiliaryGeneration) {
            return;
        }

        if (result.ok && !sameRows(m_actuatorRows, result.rows)) {
            m_actuatorRows = result.rows;
            emit actuatorCommandRowsChanged();
        }
    });
    actuatorWatcher->setFuture(m_database.fetchAllActuatorCommandsAsync());

    beginAsyncOperation();
    auto *sensorRowsWatcher = new QFutureWatcher<TableRowsQueryResult>(this);
    connect(sensorRowsWatcher, &QFutureWatcher<TableRowsQueryResult>::finished, this, [this, sensorRowsWatcher, generation]() {
        const TableRowsQueryResult result = sensorRowsWatcher->result();
        sensorRowsWatcher->deleteLater();
        endAsyncOperation();

        if (generation != m_auxiliaryGeneration) {
            return;
        }

        if (result.ok && !sameRows(m_sensorRows, result.rows)) {
            m_sensorRows = result.rows;
            emit sensorReadingRowsChanged();
        }
    });
    sensorRowsWatcher->setFuture(m_database.fetchAllSensorReadingsRowsAsync());

    beginAsyncOperation();
    auto *eventWatcher = new QFutureWatcher<TableRowsQueryResult>(this);
    connect(eventWatcher, &QFutureWatcher<TableRowsQueryResult>::finished, this, [this, eventWatcher, generation]() {
        const TableRowsQueryResult result = eventWatcher->result();
        eventWatcher->deleteLater();
        endAsyncOperation();

        if (generation != m_auxiliaryGeneration) {
            return;
        }

        if (result.ok && !sameRows(m_systemRows, result.rows)) {
            m_systemRows = result.rows;
            emit systemEventRowsChanged();
        }
    });
    eventWatcher->setFuture(m_database.fetchAllSystemEventsAsync());
}

void AppController::setDatabaseStatus(const QString &status)
{
    if (m_databaseStatus == status) {
        return;
    }

    m_databaseStatus = status;
    qWarning().noquote() << QStringLiteral("[RideManagerQml] %1").arg(status);
    emit databaseStatusChanged();
}

void AppController::setLastRefreshToNow()
{
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    if (m_lastRefreshTime == now) {
        return;
    }

    m_lastRefreshTime = now;
    emit lastRefreshTimeChanged();
}

void AppController::setCurrentRiskLevel(const QString &riskLevel)
{
    const QString normalized = riskLevel.isEmpty() ? QStringLiteral("Normal") : riskLevel;
    if (m_currentRiskLevel == normalized) {
        return;
    }

    m_currentRiskLevel = normalized;
    emit currentRiskLevelChanged();
}

void AppController::setSelectedDecisionId(const QString &decisionId)
{
    if (m_selectedDecisionId == decisionId) {
        return;
    }

    m_selectedDecisionId = decisionId;
    emit selectedDecisionIdChanged();
}

void AppController::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }

    m_busy = busy;
    emit busyChanged();
}

void AppController::beginAsyncOperation()
{
    ++m_pendingOperations;
    setBusy(true);
}

void AppController::endAsyncOperation()
{
    if (m_pendingOperations > 0) {
        --m_pendingOperations;
    }
    setBusy(m_pendingOperations > 0);
}
