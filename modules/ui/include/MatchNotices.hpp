#pragma once

#include "StateStore.hpp"
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

namespace ly27 {

// 固定节点预告与己方能量机关、双方飞镖结果面板，不推算未知增益。
class MatchNotices final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList timed READ timed NOTIFY timedChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
public:
    MatchNotices(const StateStore &store, Team team, QObject *parent = nullptr);
    QVariantList timed() const { return m_timed; }
    QVariantList results() const;
    void refresh();
signals:
    void timedChanged();
    void resultsChanged();
private:
    const StateStore &m_store;
    Team m_team;
    QString m_epoch;
    std::optional<int> m_previousElapsed;
    StateSource m_clockSource;
    QSet<int> m_crossed;
    QSet<int> m_active;
    QVariantList m_timed;
    QVariantMap m_rune;
    QVariantMap m_dart;
    quint64 m_sequence = 0;
    QTimer m_runeTimer{this};
    QTimer m_dartTimer{this};
    void observe(const QString &topic);
    void setTimed(QVariantList value);
    void clearResults();
    void showResult(QVariantMap value, bool rune);
};

}
