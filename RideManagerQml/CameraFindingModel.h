#ifndef CAMERAFINDINGMODEL_H
#define CAMERAFINDINGMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVariantList>

#include "DatabaseManager.h"

class CameraFindingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum CameraFindingRoles {
        CameraIdRole = Qt::UserRole + 1,
        LabelRole,
        ConfidenceRole,
        ObservedAtRole,
        BoxXRole,
        BoxYRole,
        BoxWidthRole,
        BoxHeightRole
    };

    explicit CameraFindingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    void setFindings(const QList<CameraFindingItem> &items);
    void clear();

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantList rowsForCamera(const QString &cameraId) const;
    Q_INVOKABLE QVariantList allRows() const;

signals:
    void countChanged();
    void findingsChanged();

private:
    QVariantMap toMap(const CameraFindingItem &item) const;

    QList<CameraFindingItem> m_items;
};

#endif // CAMERAFINDINGMODEL_H
