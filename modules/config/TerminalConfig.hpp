#pragma once

#include "TerminalIdentity.hpp"
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <optional>

// 从根目录config读取配置，包括当前角色、阵营与MqttBroker等
namespace ly27 {

enum class LogLevel { Debug, Info, Warning, Error };
QString logLevelName(LogLevel level);

struct NetworkConfig {
    QString mqttHost;
    quint16 mqttPort = 3333;
    // MQTT 身份与角色、机器人编号分开配置，实际身份需现场核实
    QString mqttClientId;
    quint16 videoPort = 3334;
    QString videoBindAddress = "127.0.0.1";
    // 留空时锁定首个合法分片的来源，停流一秒后允许重新锁定。
    QString videoSourceAddress;
};

struct TerminalConfig {
    QString sourcePath;
    TerminalIdentity identity;
    NetworkConfig network;
    QString logDirectory;
    LogLevel logLevel = LogLevel::Info;

    // 可重新加载的配置文档不包含 source_path 等诊断字段。
    QJsonObject toDocument() const;
    QJsonObject toJson() const;
};

struct ConfigResult {
    std::optional<TerminalConfig> config;
    QString error;
};

ConfigResult parseConfig(const QByteArray &json, const QString &sourcePath);
ConfigResult loadConfig(const QString &path);
ConfigResult validateConfig(const TerminalConfig &config);
// 保存到其他位置时转换相对日志路径，保持其实际指向不变。
QByteArray serializeConfig(const TerminalConfig &config, const QString &path, QString &error);
bool saveConfig(const TerminalConfig &config, const QString &path, QString &error);

} // 命名空间 ly27
