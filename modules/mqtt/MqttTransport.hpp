#pragma once

#include "TerminalConfig.hpp"
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <memory>

// Mqtt的收发唯一接口
namespace ly27 {
class MqttTransport : public QObject {
    Q_OBJECT
public:
    explicit MqttTransport(QObject *parent = nullptr);
    ~MqttTransport() override;
    bool start(const NetworkConfig &config, const QStringList &topics);
    void stop();
    // 发送指令接口
    bool publish(const QString &topic, const QByteArray &payload, int qos = 1, int *token = nullptr);
    QJsonObject snapshot() const;

signals:
    void stateChanged(const QString &state);
    void messageReceived(const QString &topic, const QByteArray &payload, qint64 receivedUtcMs,
                         qint64 receivedMonotonicNs, int qos, bool retained);
    void publishQueued(const QString &topic, int token);
    void publishDelivered(int token);
    void publishFailed(int token, const QString &error);
    void errorOccurred(const QString &error);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // 命名空间 ly27
