#pragma once

#include "TerminalIdentity.hpp"
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace ly27 {

enum class LogLevel { Debug, Info, Warning, Error };
QString logLevelName(LogLevel level);

struct NetworkConfig {
    QString mqttHost;
    quint16 mqttPort = 3333;
    // MQTT 身份与角色、机器人编号分开配置，实际身份需现场核实。
    QString mqttClientId;
    quint16 videoPort = 3334;
};

struct TerminalConfig {
    QString sourcePath;
    TerminalIdentity identity;
    NetworkConfig network;
    QString logDirectory;
    LogLevel logLevel = LogLevel::Info;

    QJsonObject toJson() const;
};

struct ConfigResult {
    std::optional<TerminalConfig> config;
    QString error;
};

ConfigResult parseConfig(const QByteArray &json, const QString &sourcePath);
ConfigResult loadConfig(const QString &path);

} // 命名空间 ly27
