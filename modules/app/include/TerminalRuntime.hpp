#pragma once

#include "TerminalConfig.hpp"
#include "MqttTransport.hpp"
#include "StateStore.hpp"
#include "CommandDispatcher.hpp"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QObject>

namespace ly27 {

// 统一管理后端生命周期。通信、状态和指令模块在这里接入
// cli和desktop均通过此对象使用后端
class TerminalRuntime : public QObject {
    Q_OBJECT
public:
    explicit TerminalRuntime(TerminalConfig config, QObject *parent = nullptr);
    ~TerminalRuntime() override;

    bool start(bool enableMqtt = false);
    void stop();
    bool isRunning() const { return m_running; }
    QJsonObject snapshot() const;
    const StateStore &stateStore() const { return m_state; }
    CommandDispatcher &commands() { return m_commands; }

signals:
    void started();
    void stopped();
    void messageObserved(const QJsonObject &observation);
    void stateChanged(const QString &topic);
    void commandObserved(const QJsonObject &event);

private:
    const TerminalConfig m_config;
    bool m_running = false;
    QString m_sessionId;
    QElapsedTimer m_uptime;
    MqttTransport m_mqtt;
    StateStore m_state;
    CommandDispatcher m_commands;
    QFile m_journal;
    bool m_mqttEnabled = false;
    qint64 m_received = 0;
    qint64 m_parsed = 0;
    qint64 m_rejected = 0;
    qint64 m_unresolved = 0;
    QJsonObject m_lastObservation;

    void receiveMessage(const QString &topic, const QByteArray &payload, qint64 receivedUtcMs,
                        qint64 receivedMonotonicNs, int qos, bool retained);
    void writeJournal(const QJsonObject &record);
};

} // 命名空间 ly27
