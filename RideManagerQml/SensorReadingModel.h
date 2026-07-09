#ifndef SENSORREADINGMODEL_H
#define SENSORREADINGMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>

#include "DatabaseManager.h"

class SensorReadingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum SensorReadingRoles {
        SensorNameRole = Qt::UserRole + 1,
        MetricRole,
        MetricDisplayRole,
        ValueRole,
        UnitRole,
        ObservedAtRole,
        ObservedAtMsRole
    };

    explicit SensorReadingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    void setReadings(const QList<SensorReadingItem> &items);
    void clear();

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantList allRows() const;

signals:
    void countChanged();
    void readingsChanged();

private:
    QVariantMap toMap(const SensorReadingItem &item) const;

    QList<SensorReadingItem> m_items;
};

#endif // SENSORREADINGMODEL_H
