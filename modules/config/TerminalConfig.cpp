#include "TerminalConfig.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>
#include <cmath>

namespace ly27 {
namespace {

class Reader {
public:
    QString error;

    bool keys(const QJsonObject &object, const QStringList &expected, const QString &path)
    {
        for (const auto &key : expected) {
            if (!object.contains(key)) {
                error = QStringLiteral("Missing configuration field: %1%2").arg(path, key);
                return false;
            }
        }
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!expected.contains(it.key())) {
                error = QStringLiteral("Unknown configuration field: %1%2").arg(path, it.key());
                return false;
            }
        }
        return true;
    }

    bool string(const QJsonObject &object, const QString &key, const QString &path,
                QString &output, bool allowEmpty = false)
    {
        const auto value = object.value(key);
        if (!value.isString() || (!allowEmpty && value.toString().trimmed().isEmpty())) {
            error = path + key + QStringLiteral(" must be a %1string")
                .arg(allowEmpty ? QString() : QStringLiteral("non-empty "));
            return false;
        }
        output = value.toString().trimmed();
        return true;
    }

    bool integer(const QJsonValue &value, const QString &path, int low, int high, int &output)
    {
        const double number = value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number
            || number < low || number > high) {
            error = QStringLiteral("%1 must be an integer in [%2, %3]").arg(path).arg(low).arg(high);
            return false;
        }
        output = static_cast<int>(number);
        return true;
    }
};

} // 命名空间

QString logLevelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug: return QStringLiteral("debug");
    case LogLevel::Info: return QStringLiteral("info");
    case LogLevel::Warning: return QStringLiteral("warning");
    case LogLevel::Error: return QStringLiteral("error");
    }
    return {};
}

QJsonObject TerminalConfig::toJson() const
{
    return {
        {QStringLiteral("source_path"), sourcePath},
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("identity"), QJsonObject{
            {QStringLiteral("role"), roleName(identity.role)},
            {QStringLiteral("team"), teamName(identity.team)},
            {QStringLiteral("station_id"), identity.stationId},
            {QStringLiteral("robot_id"), identity.robotId ? QJsonValue(*identity.robotId)
                                                        : QJsonValue(QJsonValue::Null)}}},
        {QStringLiteral("network"), QJsonObject{
            {QStringLiteral("mqtt_host"), network.mqttHost},
            {QStringLiteral("mqtt_port"), network.mqttPort},
            {QStringLiteral("mqtt_client_id"), network.mqttClientId},
            {QStringLiteral("video_port"), network.videoPort}}},
        {QStringLiteral("logging"), QJsonObject{
            {QStringLiteral("directory"), logDirectory},
            {QStringLiteral("level"), logLevelName(logLevel)}}}
    };
}

ConfigResult parseConfig(const QByteArray &json, const QString &sourcePath)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {std::nullopt, QStringLiteral("Invalid JSON at byte %1: %2")
            .arg(parseError.offset).arg(parseError.errorString())};
    }
    if (!document.isObject()) return {std::nullopt, QStringLiteral("Configuration must be a JSON object")};

    Reader reader;
    const auto root = document.object();
    if (!reader.keys(root, {"schema_version", "identity", "network", "logging"}, {}))
        return {std::nullopt, reader.error};
    int version = 0;
    if (!reader.integer(root.value("schema_version"), "schema_version", 1, 1, version))
        return {std::nullopt, reader.error};
    for (const auto &key : {"identity", "network", "logging"}) {
        if (!root.value(key).isObject())
            return {std::nullopt, QString::fromLatin1(key) + " must be an object"};
    }

    TerminalConfig config;
    config.sourcePath = QFileInfo(sourcePath).absoluteFilePath();
    const auto identity = root.value("identity").toObject();
    if (!reader.keys(identity, {"role", "team", "station_id", "robot_id"}, "identity."))
        return {std::nullopt, reader.error};
    QString role, team;
    if (!reader.string(identity, "role", "identity.", role)
        || !reader.string(identity, "team", "identity.", team)
        || !reader.string(identity, "station_id", "identity.", config.identity.stationId))
        return {std::nullopt, reader.error};
    const auto parsedRole = parseRole(role);
    const auto parsedTeam = parseTeam(team);
    if (!parsedRole) return {std::nullopt, "identity.role must be heavy, infantry or commander"};
    if (!parsedTeam) return {std::nullopt, "identity.team must be red or blue"};
    config.identity.role = *parsedRole;
    config.identity.team = *parsedTeam;
    if (!identity.value("robot_id").isNull()) {
        int robotId = 0;
        if (!reader.integer(identity.value("robot_id"), "identity.robot_id", 1, 107, robotId))
            return {std::nullopt, reader.error};
        config.identity.robotId = robotId;
    }
    if (const auto error = validateIdentity(config.identity); !error.isEmpty())
        return {std::nullopt, error};

    const auto network = root.value("network").toObject();
    if (!reader.keys(network, {"mqtt_host", "mqtt_port", "mqtt_client_id", "video_port"}, "network."))
        return {std::nullopt, reader.error};
    if (!reader.string(network, "mqtt_host", "network.", config.network.mqttHost)
        || !reader.string(network, "mqtt_client_id", "network.", config.network.mqttClientId, true))
        return {std::nullopt, reader.error};
    int mqttPort = 0, videoPort = 0;
    if (!reader.integer(network.value("mqtt_port"), "network.mqtt_port", 1, 65535, mqttPort)
        || !reader.integer(network.value("video_port"), "network.video_port", 1, 65535, videoPort))
        return {std::nullopt, reader.error};
    config.network.mqttPort = static_cast<quint16>(mqttPort);
    config.network.videoPort = static_cast<quint16>(videoPort);

    const auto logging = root.value("logging").toObject();
    if (!reader.keys(logging, {"directory", "level"}, "logging."))
        return {std::nullopt, reader.error};
    QString directory, level;
    if (!reader.string(logging, "directory", "logging.", directory)
        || !reader.string(logging, "level", "logging.", level))
        return {std::nullopt, reader.error};
    if (level == "debug") config.logLevel = LogLevel::Debug;
    else if (level == "info") config.logLevel = LogLevel::Info;
    else if (level == "warning") config.logLevel = LogLevel::Warning;
    else if (level == "error") config.logLevel = LogLevel::Error;
    else return {std::nullopt, "logging.level must be debug, info, warning or error"};
    config.logDirectory = QDir::cleanPath(QFileInfo(config.sourcePath).absoluteDir().absoluteFilePath(directory));
    return {config, {}};
}

ConfigResult loadConfig(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {std::nullopt, QStringLiteral("Cannot read configuration %1: %2")
            .arg(QFileInfo(path).absoluteFilePath(), file.errorString())};
    return parseConfig(file.readAll(), path);
}

} // 命名空间 ly27
