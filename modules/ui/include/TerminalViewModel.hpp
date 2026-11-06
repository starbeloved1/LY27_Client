#pragma once

#include "RobotMapModel.hpp"
#include "EventListModel.hpp"
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QHash>

namespace ly27 {

class TerminalViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap match READ match NOTIFY changed)
    Q_PROPERTY(QVariantMap phase READ phase NOTIFY changed)
    Q_PROPERTY(QVariantList mechanisms READ mechanisms NOTIFY changed)
    Q_PROPERTY(QVariantMap battleNotice READ battleNotice NOTIFY battleNoticeChanged)
    Q_PROPERTY(QAbstractItemModel *robots READ robots CONSTANT)
    Q_PROPERTY(QAbstractItemModel *events READ events CONSTANT)
    Q_PROPERTY(bool blueView READ blueView WRITE setBlueView NOTIFY blueViewChanged)
public:
    TerminalViewModel(const StateStore &store, TerminalIdentity identity,
                      QString redTeam, QString blueTeam, QObject *parent = nullptr);
    QVariantMap match() const { return m_match; }
    QVariantMap phase() const { return m_phase; }
    QVariantList mechanisms() const { return m_mechanisms; }
    QVariantMap battleNotice() const { return m_battleNotice; }
    QAbstractItemModel *robots() { return &m_robots; }
    QAbstractItemModel *events() { return &m_events; }
    bool blueView() const { return m_blueView; }
    void setBlueView(bool value);
    void refresh();

signals:
    void changed();
    void blueViewChanged();
    void battleNoticeChanged();

private:
    const StateStore &m_store;
    TerminalIdentity m_identity;
    QString m_redTeam;
    QString m_blueTeam;
    bool m_blueView = false;
    QVariantMap m_match;
    QVariantMap m_phase;
    QVariantList m_mechanisms;
    QString m_phaseEpoch;
    int m_lastStage = -1;
    qint64 m_startUntilNs = 0;
    QVariantMap m_battleNotice;
    QString m_battleEpoch;
    quint64 m_battleSequence = 0;
    bool m_seenRoundStart = false;
    bool m_sawKill = false;
    QHash<int, QPair<qint64, int>> m_killStreaks;
    QTimer m_battleTimer{this};
    void observeBattleEvent(const QString &topic);
    void clearBattleNotice();
    void updatePresentation(const MatchState &game, const GlobalState &global, bool paused);
    RobotMapModel m_robots;
    EventListModel m_events;
    QTimer m_timer{this};
};

}
