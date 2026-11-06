#pragma once

#include "StateStore.hpp"
#include <QAbstractListModel>

namespace ly27 {

// 事件只作为历史通知展示，不据此覆盖基地、机器人等实时状态。
class EventListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { Time = Qt::UserRole + 1, Text, Color, EventId, Parameter, TimeSource };
    static constexpr int Limit = 50;
    EventListModel(const StateStore &store, Team team, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Row { QString time, text, color, parameter, timeSource; int id = 0; };
    const StateStore &m_store;
    Team m_team;
    QList<Row> m_rows;
    QString m_session;
    quint64 m_roundRevision = 0;
    std::optional<quint64> m_totalEconomy;
    StateSource m_economySource;
    void observe(const QString &topic);
    void append(const EventNotice &event, bool live);
    void appendRow(Row row, const StateSource &source, bool live);
    void observeEconomy();
    void clear();
    Row describe(int id, const QString &parameter) const;
};

}
