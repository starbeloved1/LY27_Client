#pragma once

#include "MessageCodec.hpp"
#include "TerminalIdentity.hpp"
#include "MatchState.hpp"
#include "RoleState.hpp"

#include <QList>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <functional>
#include <optional>

// 解析字段后先进入StateStore再分发给各模块
namespace ly27 {

// 值与有效性一起返回，历史值可保留
struct StateValue {
    QJsonObject fields;
    QJsonObject source;
    QString validity = "unknown";
    QString reason = "not_received";
    std::optional<qint64> ageMs;
    std::optional<qint64> ttlMs;

    bool isFresh() const { return validity == "fresh"; }
    QJsonObject toJson() const;
};

// 只在所属 Qt 线程读写；网络回调由运行层投递，不直接访问此对象。
class StateStore : public QObject {
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;
    static constexpr int HistoryLimit = 128;

    explicit StateStore(TerminalIdentity identity, QObject *parent = nullptr,
                        Clock clock = monotonicNowNs);
    static qint64 monotonicNowNs();

    void reset(const QString &sessionId);
    void setConnected(bool connected);
    bool apply(const DecodedMessage &message, qint64 utcMs, qint64 monotonicNs,
               bool retained = false);
    StateValue topic(const QString &topic) const;
    StateValue robotTopic(int robotId, const QString &topic) const;
    StateValue robotHealth(int robotId) const;
    StateValue buff(int robotId, int type) const;
    // 业务代码使用这些 C++ 结构；上面的 Topic/JSON 接口保留用于诊断。
    MatchState matchState() const;
    GlobalState globalState() const;
    std::optional<RobotState> robot(int robotId) const;
    std::optional<RobotState> self() const;
    RoleState roleState() const;
    QList<StateNotification> notifications() const;
    QJsonObject snapshot() const;

    // 检查时间导致的有效性变化；正常运行由内部定时器驱动。
    void refresh();

signals:
    // topic 为空表示连接、会话或时间造成的批量变化。
    void changed(const QString &topic);

private:
    friend class TerminalRuntime;
    void reconfigureIdentity(const TerminalIdentity &identity);
    struct Sample {
        QString topic;
        QJsonObject fields;
        QString sessionId;
        qint64 utcMs = 0;
        qint64 monotonicNs = 0;
        qint64 ttlMs = -1;
        quint64 connection = 0;
        bool received = false;
        bool retained = false;
        std::optional<int> robotId;
        std::shared_ptr<const google::protobuf::Message> message;
        std::optional<quint32> projectedHealth;
        std::optional<quint32> round;
        quint64 roundRevision = 0;
    };

    TerminalIdentity m_identity;
    Clock m_clock;
    QTimer m_timer;
    QString m_sessionId;
    qint64 m_sessionStartedNs = 0;
    qint64 m_roundStartedNs = 0;
    std::optional<quint32> m_round;
    quint64 m_roundRevision = 0;
    bool m_connected = false;
    quint64 m_connection = 0;
    QMap<QString, Sample> m_topics;
    QMap<int, QMap<QString, Sample>> m_robots;
    QList<Sample> m_history;
    QMap<QString, QString> m_validities;

    StateValue view(const Sample &sample, qint64 nowNs) const;
    StateSource source(const Sample &sample, qint64 nowNs) const;
    void clearSamples();
    QJsonObject robotSnapshot(int robotId, qint64 nowNs) const;
    StateValue health(int robotId, qint64 nowNs) const;
    Sample healthSample(int robotId, qint64 nowNs) const;
    QMap<QString, QString> validities() const;
};

} // 命名空间 ly27
