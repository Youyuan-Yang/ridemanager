#include "SensorCatalog.h"

namespace {

const QString kUnknownMetric = QStringLiteral("--");

bool sensorMatches(const SensorDefinition &definition, const QString &sensorName)
{
    if (definition.name.compare(sensorName, Qt::CaseInsensitive) == 0) {
        return true;
    }
    return definition.name == QStringLiteral("IMU")
           && sensorName.compare(QStringLiteral("GYRO"), Qt::CaseInsensitive) == 0;
}

} // namespace

QString SensorCatalog::defaultSensorName()
{
    return QStringLiteral("RADAR");
}

QString SensorCatalog::defaultMetricName()
{
    return QStringLiteral("heart_rate");
}

QVariantList SensorCatalog::sensorNames()
{
    QVariantList names;
    for (const SensorDefinition &definition : definitions()) {
        names.append(definition.name);
    }
    return names;
}

QVariantList SensorCatalog::metricNamesForSensor(const QString &sensorName)
{
    QVariantList names;
    for (const SensorDefinition &definition : definitions()) {
        if (!sensorMatches(definition, sensorName)) {
            continue;
        }
        for (const SensorMetricDefinition &metric : definition.metrics) {
            names.append(metric.name);
        }
        return names;
    }
    return {};
}

QString SensorCatalog::sensorDescription(const QString &sensorName)
{
    for (const SensorDefinition &definition : definitions()) {
        if (sensorMatches(definition, sensorName)) {
            return QStringLiteral("%1 | %2").arg(definition.displayName, definition.description);
        }
    }
    return sensorName;
}

QString SensorCatalog::metricDisplayName(const QString &metricName)
{
    for (const SensorDefinition &definition : definitions()) {
        for (const SensorMetricDefinition &metric : definition.metrics) {
            if (metric.name == metricName) {
                if (metric.unit.isEmpty()) {
                    return metric.displayName;
                }
                return QStringLiteral("%1 (%2)").arg(metric.displayName, metric.unit);
            }
        }
    }
    return metricName.isEmpty() ? kUnknownMetric : metricName;
}

QString SensorCatalog::sensorAccentColor(const QString &sensorName)
{
    for (const SensorDefinition &definition : definitions()) {
        if (sensorMatches(definition, sensorName)) {
            return definition.accentColor;
        }
    }
    return QStringLiteral("#52a8ff");
}

const QList<SensorDefinition> &SensorCatalog::definitions()
{
    static const QList<SensorDefinition> catalog = {
        {
            QStringLiteral("RADAR"),
            QStringLiteral("毫米波雷达"),
            QStringLiteral("生命体征/距离检测"),
            QStringLiteral("#52a8ff"),
            {
                {QStringLiteral("heart_rate"), QStringLiteral("心率"), QStringLiteral("bpm")},
                {QStringLiteral("breathing_rate"), QStringLiteral("呼吸速率"), QStringLiteral("breaths/min")},
                {QStringLiteral("distance_cm"), QStringLiteral("目标距离"), QStringLiteral("cm")},
                {QStringLiteral("presence"), QStringLiteral("目标存在"), QString()},
                {QStringLiteral("stale_ms"), QStringLiteral("数据延迟"), QStringLiteral("ms")}
            }
        },
        {
            QStringLiteral("IMU"),
            QStringLiteral("IMU 运动传感器"),
            QStringLiteral("车辆倾斜/转向姿态"),
            QStringLiteral("#ffd45a"),
            {
                {QStringLiteral("roll"), QStringLiteral("横滚角"), QStringLiteral("deg")},
                {QStringLiteral("pitch"), QStringLiteral("俯仰角"), QStringLiteral("deg")},
                {QStringLiteral("yaw"), QStringLiteral("偏航角"), QStringLiteral("deg")},
                {QStringLiteral("gyro_x"), QStringLiteral("X 轴角速度"), QStringLiteral("deg/s")},
                {QStringLiteral("gyro_y"), QStringLiteral("Y 轴角速度"), QStringLiteral("deg/s")},
                {QStringLiteral("gyro_z"), QStringLiteral("Z 轴角速度"), QStringLiteral("deg/s")},
                {QStringLiteral("accel_x"), QStringLiteral("X 轴加速度"), QStringLiteral("m/s^2")},
                {QStringLiteral("accel_y"), QStringLiteral("Y 轴加速度"), QStringLiteral("m/s^2")},
                {QStringLiteral("accel_z"), QStringLiteral("Z 轴加速度"), QStringLiteral("m/s^2")}
            }
        }
    };
    return catalog;
}
