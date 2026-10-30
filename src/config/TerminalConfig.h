#pragma once

#include "core/TerminalIdentity.h"
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
    // Kept independent of role/robot ID; engine identity must be field-verified.
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

} // namespace ly27
