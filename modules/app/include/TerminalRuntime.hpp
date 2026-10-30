#pragma once

#include "TerminalConfig.hpp"
#include "MqttTransport.hpp"
#include "StateStore.hpp"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QObject>

namespace ly27 {

// 统一管理后端生命周期，通信、状态和指令模块在这里接入。
// 命令行和未来的桌面界面均通过此对象使用后端。
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

signals:
    void started();
    void stopped();
    void messageObserved(const QJsonObject &observation);
    void stateChanged(const QString &topic);

private:
    const TerminalConfig m_config;
    bool m_running = false;
    QString m_sessionId;
    QElapsedTimer m_uptime;
    MqttTransport m_mqtt;
    StateStore m_state;
    QFile m_journal;
    bool m_mqttEnabled = false;
    qint64 m_received = 0;
    qint64 m_parsed = 0;
    qint64 m_rejected = 0;
    qint64 m_unresolved = 0;
    QJsonObject m_lastObservation;

    void receiveMessage(const QString &topic, const QByteArray &payload, qint64 receivedUtcMs,
                        qint64 receivedMonotonicNs, int qos, bool retained);
};

} // 命名空间 ly27
