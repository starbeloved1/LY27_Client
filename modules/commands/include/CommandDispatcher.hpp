#pragma once

#include "CommandTypes.hpp"
#include "MqttTransport.hpp"
#include "StateStore.hpp"
#include <QMap>
#include <QTimer>
#include <functional>

// 后端提供的发送接口
namespace ly27 {

// 统一组织赛事指令
class CommandDispatcher : public QObject {
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;
    static constexpr int HistoryLimit = 128;
    static constexpr int AckTimeoutMs = 5000;

    CommandDispatcher(TerminalIdentity identity, const StateStore &state, MqttTransport &mqtt,
                      QObject *parent = nullptr, Clock clock = StateStore::monotonicNowNs);
    void start(const QString &sessionId);
    void stop();
    
    // 具体发送接口定义。发送Request，返回Submission
    CommandSubmission dispatch(const CommandRequest &request); // 全自动选择
    CommandSubmission assembly(const AssemblyRequest &request);
    CommandSubmission selectPerformance(const PerformanceRequest &request);
    CommandSubmission selectSentryMode(SentryControlMode mode);
    CommandSubmission common(const CommonRequest &request);
    CommandSubmission activateRune();
    CommandSubmission dart(const DartRequest &request);
    CommandSubmission sentry(SentryAction action);
    CommandSubmission map(const MapRequest &request);

    void observeServerResult(const DecodedMessage &message, qint64 monotonicNs, bool retained);
    QJsonObject snapshot() const;
    bool isReady() const;

signals:
    void ready();
    void observed(const QJsonObject &event);

private:
    struct Pending { QString id; qint64 submittedNs; };
    const TerminalIdentity m_identity;
    const StateStore &m_state;
    MqttTransport &m_mqtt;
    Clock m_clock;
    QTimer m_timer;
    bool m_running = false;
    QString m_sessionId;
    qint64 m_startedNs = 0;
    qint64 m_accepted = 0;
    qint64 m_rejected = 0;
    QMap<QString, qint64> m_lastSent;
    QMap<int, Pending> m_pending;
    QList<QJsonObject> m_records;
    QJsonObject m_lastServerResult;

    CommandSubmission reject(const QString &code, const QString &error, qint64 retryMs = 0);
    CommandSubmission submit(const QString &topic, const google::protobuf::Message &message,
                             qint64 intervalNs = 100000000);
    CommandSubmission requireFlag(const StateField<bool> &flag, const QString &description);
    void update(const QString &id, const QString &transport, const QString &error = {});
    void interruptPending(const QString &reason);
    void expirePending();
};

} // 命名空间 ly27
