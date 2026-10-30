#include "MessageCodec.hpp"
#include "rm2027.pb.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <google/protobuf/util/json_util.h>

// 静态库中的资源通过显式引用链接，测试和 CLI 共用同一份协议清单。
static void initializeProtocolResources()
{
    Q_INIT_RESOURCE(protocol_resources);
}

namespace ly27 {
namespace {
std::unique_ptr<google::protobuf::Message> createMessage(const QJsonObject &info)
{
    // 引用生成类型，确保静态链接时保留整个协议描述符文件。
    const auto *pool = wire::GameStatus::descriptor()->file()->pool();
    const auto name = "ly27.wire." + info.value("message").toString();
    const auto *descriptor = pool->FindMessageTypeByName(name.toStdString());
    if (!descriptor) return {};
    const auto *prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
    return prototype ? std::unique_ptr<google::protobuf::Message>(prototype->New()) : nullptr;
}

QString validatePayload(const QJsonObject &info, const google::protobuf::Message &message)
{
    if (info.contains("data_max_bytes")) {
        const auto *field = message.GetDescriptor()->FindFieldByName("data");
        const auto data = message.GetReflection()->GetString(message, field);
        if (data.size() > static_cast<size_t>(info.value("data_max_bytes").toInt()))
            return QStringLiteral("自定义载荷超过 %1 字节").arg(info.value("data_max_bytes").toInt());
    }
    return {};
}
} // 命名空间

QJsonObject MessageCodec::manifest()
{
    static const auto value = []() {
        initializeProtocolResources();
        QFile file(":/ly27/protocol/messages.json");
        if (!file.open(QIODevice::ReadOnly)) qFatal("无法读取内嵌协议清单");
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) qFatal("内嵌协议清单格式错误");
        return document.object();
    }();
    return value;
}

QJsonObject MessageCodec::topicInfo(const QString &topic)
{
    for (const auto &item : manifest().value("messages").toArray()) {
        const auto info = item.toObject();
        if (info.value("topic").toString() == topic) return info;
    }
    return {};
}

QStringList MessageCodec::inboundTopics()
{
    QStringList result;
    for (const auto &item : manifest().value("messages").toArray()) {
        const auto info = item.toObject();
        if (info.value("direction") == "inbound") result.append(info.value("topic").toString());
    }
    return result;
}

DecodedMessage MessageCodec::decode(const QString &topic, const QByteArray &payload)
{
    DecodedMessage result;
    result.topic = topic;
    const auto info = topicInfo(topic);
    if (info.isEmpty()) { result.status = "unknown_topic"; return result; }
    if (info.value("direction") != "inbound") { result.status = "wrong_direction"; return result; }
    if (payload.size() > MaxPayloadBytes) { result.status = "oversized"; return result; }
    if (info.value("schema_status") == "unresolved") {
        result.status = "unresolved_schema";
        result.error = info.value("note").toString();
        return result;
    }
    auto message = createMessage(info);
    if (!message) { result.status = "missing_descriptor"; return result; }
    if (!message->ParseFromArray(payload.constData(), static_cast<int>(payload.size()))) {
        result.status = "malformed";
        result.error = "Protobuf 二进制解析失败";
        return result;
    }
    result.error = validatePayload(info, *message);
    if (!result.error.isEmpty()) { result.status = "invalid_payload"; return result; }
    google::protobuf::util::JsonPrintOptions options;
    options.preserve_proto_field_names = true;
    std::string json;
    const auto converted = google::protobuf::util::MessageToJsonString(*message, &json, options);
    if (!converted.ok()) {
        result.status = "invalid_payload";
        result.error = QString::fromStdString(converted.ToString());
        return result;
    }
    result.fields = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object();
    result.message = std::move(message);
    result.status = "parsed";
    return result;
}

QByteArray MessageCodec::encode(const QString &topic, const google::protobuf::Message &message,
                              QString &error)
{
    error.clear();
    const auto info = topicInfo(topic);
    if (info.isEmpty() || info.value("direction") != "outbound") {
        error = "不是已定义的出站 Topic";
        return {};
    }
    if (QString::fromStdString(message.GetDescriptor()->name()) != info.value("message").toString()) {
        error = "消息类型与 Topic 不匹配";
        return {};
    }
    error = validatePayload(info, message);
    if (!error.isEmpty()) return {};
    std::string bytes;
    if (!message.SerializeToString(&bytes) || bytes.size() > MaxPayloadBytes) {
        error = "消息序列化失败或长度超限";
        return {};
    }
    // 空二进制也是合法的 Protobuf；调用者通过 error 判断成功与否。
    return QByteArray::fromStdString(bytes);
}

} // 命名空间 ly27
