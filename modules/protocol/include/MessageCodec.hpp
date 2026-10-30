#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>
#include <google/protobuf/message.h>
#include <memory>

// 按topic解码收到的载荷，或者把待发送消息编码为字节
namespace ly27 {

// 这里只输出协议消息，不保存血量、装配流程等业务状态。
struct DecodedMessage {
    QString topic;
    QString status;
    QString error;
    QJsonObject fields;
    std::shared_ptr<const google::protobuf::Message> message;
    bool isParsed() const { return status == QStringLiteral("parsed"); }
};

class MessageCodec {
public:
    static constexpr int MaxPayloadBytes = 1024 * 1024;

    static QJsonObject manifest();
    static QStringList inboundTopics();
    static QJsonObject topicInfo(const QString &topic);
    static DecodedMessage decode(const QString &topic, const QByteArray &payload);
    static QByteArray encode(const QString &topic, const google::protobuf::Message &message,
                             QString &error);
};

} // 命名空间 ly27
