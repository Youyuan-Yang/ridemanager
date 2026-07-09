#include "SensorReadingModel.h"

#include "SensorCatalog.h"

#include <QtGlobal>

namespace {

QString formatDateTime(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return QStringLiteral("--");
    }
    return dateTime.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
}

bool sameDouble(double left, double right)
{
    return qAbs(left - right) < 0.000001;
}

bool sameReadings(const QList<SensorReadingItem> &left, const QList<SensorReadingItem> &right)
{
    if (left.count() != right.count()) {
        return false;
    }
    for (int i = 0; i < left.count(); ++i) {
        const SensorReadingItem &a = left.at(i);
        const SensorReadingItem &b = right.at(i);
        if (a.sensorName != b.sensorName
            || a.metric != b.metric
            || !sameDouble(a.value, b.value)
            || a.unit != b.unit
            || a.observedAt != b.observedAt) {
            return false;
        }
    }
    return true;
}

} // namespace

SensorReadingModel::SensorReadingModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SensorReadingModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.count();
}

QVariant SensorReadingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.count()) {
        return {};
    }

    const SensorReadingItem &item = m_items.at(index.row());
    switch (role) {
    case SensorNameRole:
        return item.sensorName;
    case MetricRole:
        return item.metric;
    case MetricDisplayRole:
        return SensorCatalog::metricDisplayName(item.metric);
    case ValueRole:
        return item.value;
    case UnitRole:
        return item.unit;
    case ObservedAtRole:
        return formatDateTime(item.observedAt);
    case ObservedAtMsRole:
        return item.observedAt.isValid() ? item.observedAt.toMSecsSinceEpoch() : 0;
    default:
        return {};
    }
}

QHash<int, QByteArray> SensorReadingModel::roleNames() const
{
    return {
        {SensorNameRole, "sensorName"},
        {MetricRole, "metric"},
        {MetricDisplayRole, "metricDisplay"},
        {ValueRole, "value"},
        {UnitRole, "unit"},
        {ObservedAtRole, "observedAt"},
        {ObservedAtMsRole, "observedAtMs"}
    };
}

int SensorReadingModel::count() const
{
    return m_items.count();
}

void SensorReadingModel::setReadings(const QList<SensorReadingItem> &items)
{
    if (sameReadings(m_items, items)) {
        return;
    }

    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
    emit readingsChanged();
}

void SensorReadingModel::clear()
{
    setReadings({});
}

QVariantMap SensorReadingModel::get(int row) const
{
    if (row < 0 || row >= m_items.count()) {
        return {};
    }
    return toMap(m_items.at(row));
}

QVariantList SensorReadingModel::allRows() const
{
    QVariantList rows;
    rows.reserve(m_items.count());
    for (const SensorReadingItem &item : m_items) {
        rows.append(toMap(item));
    }
    return rows;
}

QVariantMap SensorReadingModel::toMap(const SensorReadingItem &item) const
{
    return {
        {QStringLiteral("sensorName"), item.sensorName},
        {QStringLiteral("metric"), item.metric},
        {QStringLiteral("metricDisplay"), SensorCatalog::metricDisplayName(item.metric)},
        {QStringLiteral("value"), item.value},
        {QStringLiteral("unit"), item.unit},
        {QStringLiteral("observedAt"), formatDateTime(item.observedAt)},
        {QStringLiteral("observedAtMs"), item.observedAt.isValid() ? item.observedAt.toMSecsSinceEpoch() : 0}
    };
}
