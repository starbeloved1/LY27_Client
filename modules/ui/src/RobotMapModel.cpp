#include "RobotMapModel.hpp"

#include <algorithm>
#include <cmath>

namespace ly27 {

RobotMapModel::RobotMapModel(const StateStore &store, const TerminalIdentity &identity, QObject *parent)
    : QAbstractListModel(parent), m_store(store), m_identity(identity)
{
    m_rows.resize(8);
    refresh(identity.team == Team::Blue);
}

int RobotMapModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant RobotMapModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    return m_rows.at(index.row()).value(QString::fromLatin1(roleNames().value(role)));
}

QHash<int, QByteArray> RobotMapModel::roleNames() const
{
    return {{RobotId, "robotId"}, {Number, "robotNumber"}, {Blue, "isBlue"},
            {Self, "isSelf"}, {MapX, "mapX"}, {MapY, "mapY"}, {Heading, "heading"},
            {PositionKnown, "positionKnown"}, {PositionFresh, "positionFresh"},
            {HpText, "hpText"}, {LevelText, "levelText"}, {HpKnown, "hpKnown"},
            {HpRatio, "hpRatio"}};
}

QPointF RobotMapModel::projectPosition(double x, double y, bool blueView)
{
    // 图片内场地围挡的近似范围；这是显示标定，不是 AI 图片的精确测量结果。
    double u = (24.0 + std::clamp(x / 28.0, 0.0, 1.0) * 1614.0) / 1662.0;
    double v = (24.0 + (1.0 - std::clamp(y / 15.0, 0.0, 1.0)) * 897.0) / 946.0;
    if (blueView) { u = 1.0 - u; v = 1.0 - v; }
    return {u, v};
}

void RobotMapModel::refresh(bool blueView)
{
    const int ids[] = {1, 3, 4, 7, 101, 103, 104, 107};
    for (int row = 0; row < 8; ++row) {
        const int id = ids[row];
        const auto robot = m_store.robot(id);
        QVariantMap values{{"robotId", id}, {"robotNumber", id % 100}, {"isBlue", id >= 100},
                           {"isSelf", m_identity.robotId && *m_identity.robotId == id},
                           {"mapX", 0.0}, {"mapY", 0.0}, {"heading", 0.0},
                           {"positionKnown", false}, {"positionFresh", false},
                           {"hpText", QStringLiteral("—")}, {"levelText", QStringLiteral("Lv.—")},
                           {"hpKnown", false}, {"hpRatio", -1.0}};
        if (robot) {
            const auto &position = robot->position;
            const auto &source = position.x.source;
            // 过期位置短暂保留并淡化，断线、换局、retained 和超过五秒的位置不画。
            const bool canShow = position.x.value && position.y.value && source.received &&
                source.validity != StateValidity::Disconnected && source.validity != StateValidity::Retained &&
                source.reason != QStringLiteral("previous_connection") &&
                source.ageMs && *source.ageMs < 5000;
            if (canShow) {
                const auto point = projectPosition(*position.x.value, *position.y.value, blueView);
                values["mapX"] = point.x();
                values["mapY"] = point.y();
                values["positionKnown"] = true;
                values["positionFresh"] = position.x.isFresh() && position.y.isFresh();
            }
            if (position.yawDegrees.isFresh())
                values["heading"] = *position.yawDegrees.value + (blueView ? 180.0 : 0.0);
            if (robot->currentHealth.isFresh()) {
                values["hpKnown"] = true;
                values["hpText"] = QString::number(*robot->currentHealth.value);
                if (robot->maxHealth.isFresh() && *robot->maxHealth.value > 0)
                    values["hpRatio"] = std::clamp(double(*robot->currentHealth.value) / *robot->maxHealth.value, 0.0, 1.0);
            }
            if (robot->level.isFresh())
                values["levelText"] = QStringLiteral("Lv.%1").arg(*robot->level.value);
        }
        if (m_rows[row] != values) {
            m_rows[row] = values;
            emit dataChanged(index(row, 0), index(row, 0));
        }
    }
}

}
