#ifndef SENSORCATALOG_H
#define SENSORCATALOG_H

#include <QList>
#include <QString>
#include <QVariantList>

struct SensorMetricDefinition
{
    QString name;
    QString displayName;
    QString unit;
};

struct SensorDefinition
{
    QString name;
    QString displayName;
    QString description;
    QString accentColor;
    QList<SensorMetricDefinition> metrics;
};

class SensorCatalog
{
public:
    static QString defaultSensorName();
    static QString defaultMetricName();
    static QVariantList sensorNames();
    static QVariantList metricNamesForSensor(const QString &sensorName);
    static QString sensorDescription(const QString &sensorName);
    static QString metricDisplayName(const QString &metricName);
    static QString sensorAccentColor(const QString &sensorName);

private:
    static const QList<SensorDefinition> &definitions();
};

#endif // SENSORCATALOG_H
