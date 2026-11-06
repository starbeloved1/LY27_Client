#pragma once

#include "CommandDispatcher.hpp"
#include <QPointF>
#include <QPointer>
#include <QVariantList>

namespace ly27 {

// 地图点选、指令提交和同步标记的展示适配；不创建通信对象或解析原始消息。
class TacticalMapController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool commander READ commander CONSTANT)
    Q_PROPERTY(int selectedReceiver READ selectedReceiver NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY markersChanged)
    Q_PROPERTY(QVariantMap markerStates READ markerStates NOTIFY markersChanged)
    Q_PROPERTY(QVariantList facilities READ facilities CONSTANT)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
public:
    static constexpr int MarkerLifetimeMs = 10000;
    TacticalMapController(const StateStore &store, TerminalIdentity identity, QObject *parent = nullptr,
                          StateStore::Clock clock = StateStore::monotonicNowNs);
    bool commander() const { return m_identity.role == Role::Commander; }
    int selectedReceiver() const { return m_selected; }
    QVariantMap pending() const { return m_pendingView; }
    QVariantList markers() const { return m_markerViews; }
    QVariantMap markerStates() const { return m_markerStates; }
    QVariantList facilities() const;
    QString feedback() const { return m_feedback; }
    void setCommandDispatcher(CommandDispatcher *commands);
    void setBlueView(bool blue);
    Q_INVOKABLE void clickRobot(int id);
    Q_INVOKABLE void clickFacility(int id);
    // 传入实际底图区域的归一化坐标，包含当前视角翻转。
    Q_INVOKABLE void clickMap(qreal x, qreal y);
    Q_INVOKABLE bool chooseMarker(int type);
    Q_INVOKABLE void cancel();
    void refresh();
    static QPointF unprojectPosition(QPointF point, bool blueView);

signals:
    void selectionChanged();
    void pendingChanged();
    void markersChanged();
    void feedbackChanged();

private:
    struct Target { int id = 0; QPointF position; };
    struct Marker { Target target; int type = 0; qint64 expiresNs = 0; };
    const StateStore &m_store;
    TerminalIdentity m_identity;
    StateStore::Clock m_clock;
    QPointer<CommandDispatcher> m_commands;
    QMetaObject::Connection m_commandObservation;
    QTimer m_timer{this};
    bool m_blueView = false;
    int m_selected = 0;
    std::optional<Target> m_pending;
    QVariantMap m_pendingView;
    QList<int> m_options;
    QMap<int, Marker> m_active;
    QVariantList m_markerViews;
    QVariantMap m_markerStates;
    QString m_feedback;
    QString m_epoch;
    QString m_submission;
    void feedback(const QString &text);
    void beginTarget(const Target &target, const QList<int> &options);
    void updatePending();
    void observeMarker();
    void clearSession();
    std::optional<QPointF> targetPosition(const Target &target) const;
};

}
