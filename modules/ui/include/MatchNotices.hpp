#pragma once

#include "StateStore.hpp"
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

namespace ly27 {

// 27 规则的机会发放预告与实时事件提示，不推算剩余机会或虚构事件。
class MatchNotices final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList timed READ timed NOTIFY timedChanged)
    Q_PROPERTY(QVariantMap event READ event NOTIFY eventChanged)
public:
    MatchNotices(const StateStore &store, Team team, QObject *parent = nullptr);
    QVariantList timed() const { return m_timed; }
    QVariantMap event() const { return m_event; }
    void refresh();
signals:
    void timedChanged();
    void eventChanged();
private:
    const StateStore &m_store;
    Team m_team;
    QString m_epoch;
    std::optional<int> m_previousElapsed;
    StateSource m_clockSource;
    QSet<int> m_crossed;
    QSet<int> m_active;
    QVariantList m_timed;
    QVariantMap m_event;
    quint64 m_sequence = 0;
    QTimer m_eventTimer{this};
    void observe(const QString &topic);
    void setTimed(QVariantList value);
    void clearEvent();
};

}
