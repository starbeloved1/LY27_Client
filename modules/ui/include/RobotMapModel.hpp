#pragma once

#include "StateStore.hpp"
#include <QAbstractListModel>
#include <QPointF>
#include <QVariantMap>

namespace ly27 {

class RobotMapModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        RobotId = Qt::UserRole + 1, Number, Blue, Self, MapX, MapY, Heading,
        PositionKnown, PositionFresh, HpText, LevelText, HpKnown, HpRatio, Buffs
    };
    explicit RobotMapModel(const StateStore &store, const TerminalIdentity &identity,
                           QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void refresh(bool blueView);
    static QPointF projectPosition(double x, double y, bool blueView = false);

private:
    const StateStore &m_store;
    TerminalIdentity m_identity;
    QList<QVariantMap> m_rows;
};

}
