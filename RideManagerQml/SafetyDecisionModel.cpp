#include "SafetyDecisionModel.h"

namespace {

QString formatDateTime(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return QStringLiteral("--");
    }
    return dateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

bool sameDecisions(const QList<SafetyDecisionItem> &left, const QList<SafetyDecisionItem> &right)
{
    if (left.count() != right.count()) {
        return false;
    }
    for (int i = 0; i < left.count(); ++i) {
        if (left.at(i).decisionId != right.at(i).decisionId
            || left.at(i).riskLevel != right.at(i).riskLevel
            || left.at(i).decidedAt != right.at(i).decidedAt) {
            return false;
        }
    }
    return true;
}

} // namespace

SafetyDecisionModel::SafetyDecisionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SafetyDecisionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.count();
}

QVariant SafetyDecisionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.count()) {
        return {};
    }

    const SafetyDecisionItem &item = m_items.at(index.row());
    switch (role) {
    case DecisionIdRole:
        return item.decisionId;
    case RiskLevelRole:
        return item.riskLevel;
    case DecidedAtRole:
        return formatDateTime(item.decidedAt);
    default:
        return {};
    }
}

QHash<int, QByteArray> SafetyDecisionModel::roleNames() const
{
    return {
        {DecisionIdRole, "decisionId"},
        {RiskLevelRole, "riskLevel"},
        {DecidedAtRole, "decidedAt"}
    };
}

int SafetyDecisionModel::count() const
{
    return m_items.count();
}

void SafetyDecisionModel::setDecisions(const QList<SafetyDecisionItem> &items)
{
    if (sameDecisions(m_items, items)) {
        return;
    }

    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
}

void SafetyDecisionModel::clear()
{
    setDecisions({});
}

QString SafetyDecisionModel::riskLevelForDecision(const QString &decisionId) const
{
    for (const SafetyDecisionItem &item : m_items) {
        if (item.decisionId == decisionId) {
            return item.riskLevel;
        }
    }
    return QStringLiteral("Normal");
}

QVariantMap SafetyDecisionModel::get(int row) const
{
    if (row < 0 || row >= m_items.count()) {
        return {};
    }
    return toMap(m_items.at(row));
}

QVariantMap SafetyDecisionModel::toMap(const SafetyDecisionItem &item) const
{
    return {
        {QStringLiteral("decisionId"), item.decisionId},
        {QStringLiteral("riskLevel"), item.riskLevel},
        {QStringLiteral("decidedAt"), formatDateTime(item.decidedAt)}
    };
}
