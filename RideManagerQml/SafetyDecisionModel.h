#ifndef SAFETYDECISIONMODEL_H
#define SAFETYDECISIONMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>

#include "DatabaseManager.h"

class SafetyDecisionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum SafetyDecisionRoles {
        DecisionIdRole = Qt::UserRole + 1,
        RiskLevelRole,
        DecidedAtRole
    };

    explicit SafetyDecisionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    void setDecisions(const QList<SafetyDecisionItem> &items);
    void clear();
    QString riskLevelForDecision(const QString &decisionId) const;

    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void countChanged();

private:
    QVariantMap toMap(const SafetyDecisionItem &item) const;

    QList<SafetyDecisionItem> m_items;
};

#endif // SAFETYDECISIONMODEL_H
