#pragma once

#include "RobotMapModel.hpp"
#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace ly27 {

class TerminalViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap match READ match NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel *robots READ robots CONSTANT)
    Q_PROPERTY(bool blueView READ blueView WRITE setBlueView NOTIFY blueViewChanged)
public:
    TerminalViewModel(const StateStore &store, TerminalIdentity identity,
                      QString redTeam, QString blueTeam, QObject *parent = nullptr);
    QVariantMap match() const { return m_match; }
    QAbstractItemModel *robots() { return &m_robots; }
    bool blueView() const { return m_blueView; }
    void setBlueView(bool value);
    void refresh();

signals:
    void changed();
    void blueViewChanged();

private:
    const StateStore &m_store;
    TerminalIdentity m_identity;
    QString m_redTeam;
    QString m_blueTeam;
    bool m_blueView = false;
    QVariantMap m_match;
    RobotMapModel m_robots;
    QTimer m_timer{this};
};

}
