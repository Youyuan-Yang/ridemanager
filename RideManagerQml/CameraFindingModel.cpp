#include "CameraFindingModel.h"

#include <QtGlobal>

namespace {

QString formatDateTime(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return QStringLiteral("--");
    }
    return dateTime.toLocalTime().toString(QStringLiteral("HH:mm:ss.zzz"));
}

bool sameDouble(double left, double right)
{
    return qAbs(left - right) < 0.000001;
}

bool sameFindings(const QList<CameraFindingItem> &left, const QList<CameraFindingItem> &right)
{
    if (left.count() != right.count()) {
        return false;
    }
    for (int i = 0; i < left.count(); ++i) {
        const CameraFindingItem &a = left.at(i);
        const CameraFindingItem &b = right.at(i);
        if (a.cameraId != b.cameraId
            || a.label != b.label
            || !sameDouble(a.confidence, b.confidence)
            || a.observedAt != b.observedAt
            || !sameDouble(a.boxX, b.boxX)
            || !sameDouble(a.boxY, b.boxY)
            || !sameDouble(a.boxWidth, b.boxWidth)
            || !sameDouble(a.boxHeight, b.boxHeight)) {
            return false;
        }
    }
    return true;
}

QString normalizedCameraId(QString cameraId)
{
    cameraId = cameraId.trimmed().toUpper();
    cameraId.replace(QStringLiteral("-"), QStringLiteral("_"));
    cameraId.replace(QStringLiteral("CAMERA_"), QString());
    cameraId.replace(QStringLiteral("CAM_"), QString());
    return cameraId;
}

bool sameCameraChannel(const QString &left, const QString &right)
{
    const QString actual = normalizedCameraId(left);
    const QString requested = normalizedCameraId(right);
    if (actual == requested) {
        return true;
    }
    if (requested == QStringLiteral("FRONT")) {
        return actual.contains(QStringLiteral("FRONT"));
    }
    if (requested == QStringLiteral("FACE")) {
        return actual.contains(QStringLiteral("FACE"))
               || actual.contains(QStringLiteral("DRIVER"))
               || actual.contains(QStringLiteral("CABIN"));
    }
    if (requested == QStringLiteral("BACK")) {
        return actual.contains(QStringLiteral("BACK"))
               || actual.contains(QStringLiteral("REAR"));
    }
    return false;
}

} // namespace

CameraFindingModel::CameraFindingModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CameraFindingModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.count();
}

QVariant CameraFindingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.count()) {
        return {};
    }

    const CameraFindingItem &item = m_items.at(index.row());
    switch (role) {
    case CameraIdRole:
        return item.cameraId;
    case LabelRole:
        return item.label;
    case ConfidenceRole:
        return item.confidence;
    case ObservedAtRole:
        return formatDateTime(item.observedAt);
    case BoxXRole:
        return item.boxX;
    case BoxYRole:
        return item.boxY;
    case BoxWidthRole:
        return item.boxWidth;
    case BoxHeightRole:
        return item.boxHeight;
    default:
        return {};
    }
}

QHash<int, QByteArray> CameraFindingModel::roleNames() const
{
    return {
        {CameraIdRole, "cameraId"},
        {LabelRole, "label"},
        {ConfidenceRole, "confidence"},
        {ObservedAtRole, "observedAt"},
        {BoxXRole, "boxX"},
        {BoxYRole, "boxY"},
        {BoxWidthRole, "boxWidth"},
        {BoxHeightRole, "boxHeight"}
    };
}

int CameraFindingModel::count() const
{
    return m_items.count();
}

void CameraFindingModel::setFindings(const QList<CameraFindingItem> &items)
{
    if (sameFindings(m_items, items)) {
        return;
    }

    beginResetModel();
    m_items = items;
    endResetModel();
    emit findingsChanged();
    emit countChanged();
}

void CameraFindingModel::clear()
{
    setFindings({});
}

QVariantMap CameraFindingModel::get(int row) const
{
    if (row < 0 || row >= m_items.count()) {
        return {};
    }
    return toMap(m_items.at(row));
}

QVariantList CameraFindingModel::rowsForCamera(const QString &cameraId) const
{
    QVariantList rows;
    for (const CameraFindingItem &item : m_items) {
        if (sameCameraChannel(item.cameraId, cameraId)) {
            rows.append(toMap(item));
        }
    }
    return rows;
}

QVariantList CameraFindingModel::allRows() const
{
    QVariantList rows;
    rows.reserve(m_items.count());
    for (const CameraFindingItem &item : m_items) {
        rows.append(toMap(item));
    }
    return rows;
}

QVariantMap CameraFindingModel::toMap(const CameraFindingItem &item) const
{
    const QString bbox = QStringLiteral("%1, %2, %3, %4")
                             .arg(item.boxX, 0, 'f', 2)
                             .arg(item.boxY, 0, 'f', 2)
                             .arg(item.boxWidth, 0, 'f', 2)
                             .arg(item.boxHeight, 0, 'f', 2);

    return {
        {QStringLiteral("cameraId"), item.cameraId},
        {QStringLiteral("label"), item.label},
        {QStringLiteral("confidence"), item.confidence},
        {QStringLiteral("observedAt"), formatDateTime(item.observedAt)},
        {QStringLiteral("boxX"), item.boxX},
        {QStringLiteral("boxY"), item.boxY},
        {QStringLiteral("boxWidth"), item.boxWidth},
        {QStringLiteral("boxHeight"), item.boxHeight},
        {QStringLiteral("bbox"), bbox}
    };
}
