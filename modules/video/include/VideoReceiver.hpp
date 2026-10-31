#pragma once

#include "VideoTypes.hpp"
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <memory>
#include <optional>

// 组织整个流程
namespace ly27 {

class VideoReceiver : public QObject {
    Q_OBJECT
public:
    explicit VideoReceiver(QObject *parent = nullptr);
    ~VideoReceiver() override;
    // 默认仅监听回环地址，启用现场网卡需显式配置。port=0 只用于本地测试。
    bool start(quint16 port, const QString &sessionId, QString &error,
               const QHostAddress &bindAddress = QHostAddress::LocalHost,
               const QHostAddress &sourceAddress = {});
    void stop();
    // 新后端会话开始前清空上一会话的图像、统计和错误。
    void reset();
    std::optional<VideoFrame> latestFrame() const;
    QJsonObject snapshot() const;

signals:
    // 通知合并为至多每 16 ms 一次，消费者按需取最新帧，避免跨线程积压图像。
    void frameAvailable();
    void stateChanged(const QString &state);

private:
    struct Impl;
    std::unique_ptr<Impl> m;
    void poll();
};

} // 命名空间 ly27
