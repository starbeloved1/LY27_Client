#include "TerminalRuntime.hpp"
#include "MessageCodec.hpp"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QUuid>
#include <QFileInfo>
#include <QScopedValueRollback>
#include <QThread>
#include <utility>

namespace ly27 {

TerminalRuntime::TerminalRuntime(TerminalConfig config, QObject *parent, RuntimeLogger *logger)
    : QObject(parent), m_config(std::move(config)), m_logger(logger), m_mqtt(this), m_state(m_config.identity, this),
      m_commands(m_config.identity, m_state, m_mqtt, this), m_video(this)
{
    m_configDebounce.setSingleShot(true);
    m_configDebounce.setInterval(150);
    connect(&m_configDebounce, &QTimer::timeout, this, &TerminalRuntime::checkWatchedConfig);
    const auto changed = [this](const QString &) { if (m_watchingConfig) m_configDebounce.start(); };
    connect(&m_configWatcher, &QFileSystemWatcher::fileChanged, this, changed);
    connect(&m_configWatcher, &QFileSystemWatcher::directoryChanged, this, changed);
    connect(&m_mqtt, &MqttTransport::stateChanged, this, [this](const QString &state) {
        qInfo().noquote() << "mqtt.state=" + state;
        m_state.setConnected(state == "subscribing" || state == "ready");
    });
    connect(&m_mqtt, &MqttTransport::errorOccurred, this, [](const QString &error) {
        qWarning().noquote() << "mqtt.error=" + error;
    });
    connect(&m_video, &VideoReceiver::frameAvailable, this, &TerminalRuntime::videoFrameAvailable);
    connect(&m_video, &VideoReceiver::stateChanged, this, [this](const QString &state) {
        qInfo().noquote() << "video.state=" + state;
        emit videoStateChanged(state);
    });
    connect(&m_mqtt, &MqttTransport::messageReceived, this, &TerminalRuntime::receiveMessage);
    connect(&m_state, &StateStore::changed, this, &TerminalRuntime::stateChanged);
    connect(&m_commands, &CommandDispatcher::observed, this, [this](const QJsonObject &event) {
        auto record = event;
        record.insert("direction", event.value("event") == "command_submitted" ? "tx" : "command_event");
        record.insert("observed_utc_ms", QDateTime::currentMSecsSinceEpoch());
        writeJournal(record);
        qInfo().noquote() << "commands.event=" + event.value("event").toString();
        emit commandObserved(event);
    });
}

TerminalRuntime::~TerminalRuntime() { stop(); }

bool TerminalRuntime::start(bool enableMqtt, bool enableVideo)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_operating) return false;
    if (m_running) return true;
    QScopedValueRollback<bool> operation(m_operating, true);
    QString error;
    if (!startBackend(enableMqtt, enableVideo, error)) {
        qCritical().noquote() << error;
        return false;
    }
    emit started();
    return true;
}

bool TerminalRuntime::startBackend(bool enableMqtt, bool enableVideo, QString &error)
{
    error.clear();
    m_videoEnabled = enableVideo;
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_mqttEnabled = enableMqtt;
    m_received = m_parsed = m_rejected = m_unresolved = 0;
    m_lastObservation = {};
    m_state.reset(m_sessionId);
    m_video.reset();
    if (enableMqtt) {
        if (m_config.network.mqttClientId.isEmpty()) {
            error = "启用 MQTT 前必须填写 network.mqtt_client_id";
            return false;
        }
        if (!QDir().mkpath(m_config.logDirectory)) {
            error = "无法创建 MQTT 记录目录";
            return false;
        }
        m_journal.setFileName(QDir(m_config.logDirectory).filePath("mqtt_" + m_sessionId + ".jsonl"));
        if (!m_journal.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            error = "无法创建 MQTT 原始消息记录：" + m_journal.errorString();
            return false;
        }
        if (!m_mqtt.start(m_config.network, MessageCodec::inboundTopics())) {
            error = "MQTT 启动失败：" + m_mqtt.snapshot().value("last_error").toString();
            m_journal.close();
            return false;
        }
    }
    if (enableVideo) {
        if (!m_video.start(m_config.network.videoPort, m_sessionId, error,
                           QHostAddress(m_config.network.videoBindAddress),
                           QHostAddress(m_config.network.videoSourceAddress))) {
            error = "图传启动失败：" + error;
            m_mqtt.stop();
            m_journal.close();
            m_state.setConnected(false);
            return false;
        }
    }
    m_uptime.start();
    m_running = true;
    m_commands.start(m_sessionId);
    qInfo().noquote() << "runtime.started session=" + m_sessionId
                     << "station=" + m_config.identity.stationId
                     << "role=" + roleName(m_config.identity.role)
                     << "team=" + teamName(m_config.identity.team);
    return true;
}

void TerminalRuntime::stop()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_operating || !m_running) return;
    QScopedValueRollback<bool> operation(m_operating, true);
    stopBackend();
    emit stopped();
}

void TerminalRuntime::stopBackend()
{
    if (!m_running) return;
    m_running = false;
    m_commands.stop();
    m_mqtt.stop();
    m_state.setConnected(false);
    m_video.stop();
    m_journal.close();
    qInfo().noquote() << "runtime.stopped session=" + m_sessionId
                     << "uptime_ms=" + QString::number(m_uptime.elapsed());
    m_uptime.invalidate();
}

ConfigApplyResult TerminalRuntime::rejectConfig(const QString &error, const QString &rollbackError, bool rolledBack)
{
    QScopedValueRollback<bool> operation(m_operating, true);
    m_lastConfigError = error;
    if (!rollbackError.isEmpty()) m_lastConfigError += "；恢复旧配置失败：" + rollbackError;
    qWarning().noquote() << "config.rejected=" + m_lastConfigError;
    emit configurationChangeFailed(error, rollbackError);
    return {false, false, rolledBack, error, rollbackError};
}

void TerminalRuntime::installConfig(const TerminalConfig &config)
{
    Q_ASSERT(!m_running);
    m_config = config;
    m_state.reconfigureIdentity(m_config.identity);
    m_commands.reconfigureIdentity(m_config.identity);
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_state.reset(m_sessionId);
    m_video.reset();
    m_mqttEnabled = m_videoEnabled = false;
    m_received = m_parsed = m_rejected = m_unresolved = 0;
    m_lastObservation = {};
}

ConfigApplyResult TerminalRuntime::applyConfig(const TerminalConfig &candidate)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_operating) return {false, false, false, "后端正在执行启动、停止或配置变更", {}};
    QScopedValueRollback<bool> operation(m_operating, true);
    const auto checked = validateConfig(candidate);
    if (!checked.config) return rejectConfig(checked.error);
    const auto next = *checked.config;
    if (m_running && m_mqttEnabled && next.network.mqttClientId.isEmpty())
        return rejectConfig("启用 MQTT 时 network.mqtt_client_id 不能为空");
    if (next.toJson() == m_config.toJson()) {
        m_lastConfigError.clear();
        return {true, false, false, {}, {}};
    }
    const auto previous = m_config;
    const bool wasRunning = m_running, mqtt = m_mqttEnabled, video = m_videoEnabled;
    stopBackend();
    installConfig(next);
    QString error;
    bool ok = !wasRunning || startBackend(mqtt, video, error);
    if (ok && m_logger) ok = m_logger->reconfigure(next.logDirectory, next.logLevel, error);
    if (!ok) {
        // 不恢复旧缓存或重发旧指令；恢复配置时也建立新的会话。
        stopBackend();
        installConfig(previous);
        QString rollbackError;
        const bool recovered = !wasRunning || startBackend(mqtt, video, rollbackError);
        return rejectConfig(error, rollbackError, recovered);
    }
    m_lastConfigError.clear();
    m_configAppliedUtcMs = QDateTime::currentMSecsSinceEpoch();
    if (m_watchingConfig) {
        QString watchError;
        if (!refreshConfigWatch(watchError)) {
            m_watchingConfig = false;
            m_lastConfigError = watchError;
            qWarning().noquote() << watchError;
        }
    }
    qInfo().noquote() << "config.applied path=" + m_config.sourcePath
                     << "role=" + roleName(m_config.identity.role)
                     << "session=" + m_sessionId;
    emit configurationChanged();
    return {true, wasRunning, false, {}, {}};
}

ConfigApplyResult TerminalRuntime::reloadConfig(const QString &path)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_operating) return {false, false, false, "后端正在执行配置变更", {}};
    const auto loaded = loadConfig(path.isEmpty() ? m_config.sourcePath : path);
    if (!loaded.config) return rejectConfig(loaded.error);
    return applyConfig(*loaded.config);
}

ConfigApplyResult TerminalRuntime::switchIdentity(const TerminalIdentity &identity, const QString &mqttClientId)
{
    auto next = m_config;
    next.identity = identity;
    next.network.mqttClientId = mqttClientId;
    return applyConfig(next);
}

bool TerminalRuntime::saveCurrentConfig(const QString &path, QString &error) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_operating) { error = "后端正在执行配置变更"; return false; }
    return ly27::saveConfig(m_config, path, error);
}

bool TerminalRuntime::refreshConfigWatch(QString &error)
{
    const auto oldFiles = m_configWatcher.files(), oldDirectories = m_configWatcher.directories();
    if (!oldFiles.isEmpty()) m_configWatcher.removePaths(oldFiles);
    if (!oldDirectories.isEmpty()) m_configWatcher.removePaths(oldDirectories);
    const QFileInfo info(m_config.sourcePath);
    if (m_config.sourcePath.isEmpty() || !m_configWatcher.addPath(info.absolutePath())) {
        error = "无法监听配置目录：" + info.absolutePath(); return false;
    }
    if (info.exists()) m_configWatcher.addPath(info.absoluteFilePath());
    QFile file(m_config.sourcePath);
    m_watchedContents = file.open(QIODevice::ReadOnly) ? file.read(65537) : QByteArray();
    return true;
}

bool TerminalRuntime::setConfigWatching(bool enabled, QString &error)
{
    Q_ASSERT(QThread::currentThread() == thread());
    error.clear();
    if (m_operating) { error = "后端正在执行配置变更"; return false; }
    m_configDebounce.stop();
    if (enabled) {
        if (!refreshConfigWatch(error)) { m_watchingConfig = false; return false; }
        m_watchingConfig = true;
    } else {
        m_watchingConfig = false;
        const auto files = m_configWatcher.files(), directories = m_configWatcher.directories();
        if (!files.isEmpty()) m_configWatcher.removePaths(files);
        if (!directories.isEmpty()) m_configWatcher.removePaths(directories);
    }
    return true;
}

void TerminalRuntime::checkWatchedConfig()
{
    if (!m_watchingConfig) return;
    const auto path = m_config.sourcePath;
    // 同时监听父目录以兼容编辑器的原子替换保存；内容不变时不重启后端。
    if (QFileInfo::exists(path) && !m_configWatcher.files().contains(path)) m_configWatcher.addPath(path);
    QFile file(path);
    const auto contents = file.open(QIODevice::ReadOnly) ? file.read(65537) : QByteArray();
    if (contents == m_watchedContents) return;
    m_watchedContents = contents;
    reloadConfig();
}

QJsonObject TerminalRuntime::snapshot() const
{
    return {
        {"application", "LY27 Client"},
        {"version", LY27_VERSION},
        {"state", m_running ? "running" : "stopped"},
        {"session_id", m_sessionId},
        {"uptime_ms", m_running ? m_uptime.elapsed() : qint64(0)},
        {"config", m_config.toJson()},
        {"configuration", QJsonObject{{"watching", m_watchingConfig},
            {"last_error", m_lastConfigError}, {"applied_utc_ms", m_configAppliedUtcMs}}},
        {"modules", QJsonObject{{"mqtt", m_mqttEnabled ? m_mqtt.snapshot().value("state") : QJsonValue("disabled")},
                               {"video", m_video.snapshot().value("state")},
                               {"commands", m_commands.snapshot().value("state")}}},
        {"mqtt", m_mqtt.snapshot()},
        {"video", m_video.snapshot()},
        {"business_state", m_state.snapshot()},
        {"commands", m_commands.snapshot()},
        {"protocol", QJsonObject{{"received", m_received}, {"parsed", m_parsed},
            {"rejected", m_rejected}, {"unresolved", m_unresolved},
            {"journal", m_mqttEnabled ? m_journal.fileName() : QString()},
            {"last_observation", m_lastObservation}}}
    };
}

void TerminalRuntime::receiveMessage(const QString &topic, const QByteArray &payload,
                                    qint64 receivedUtcMs, qint64 receivedMonotonicNs,
                                    int qos, bool retained)
{
    if (!m_running) return;
    ++m_received;
    const auto decoded = MessageCodec::decode(topic, payload);
    if (decoded.isParsed()) ++m_parsed;
    else if (decoded.status == "unresolved_schema") ++m_unresolved;
    else ++m_rejected;
    m_lastObservation = {{"topic", topic}, {"status", decoded.status}, {"error", decoded.error},
        {"received_utc_ms", receivedUtcMs},
        {"received_monotonic_ns", QString::number(receivedMonotonicNs)},
        {"bytes", payload.size()}, {"qos", qos}, {"retained", retained}, {"fields", decoded.fields}};
    auto record = m_lastObservation;
    record.insert("session_id", m_sessionId);
    record.insert("direction", "rx");
    record.insert("payload_base64", QString::fromLatin1(payload.toBase64()));
    writeJournal(record);
    if (!decoded.isParsed()) qWarning().noquote() << "protocol.status=" + decoded.status << "topic=" + topic;
    m_state.apply(decoded, receivedUtcMs, receivedMonotonicNs, retained);
    m_commands.observeServerResult(decoded, receivedMonotonicNs, retained);
    emit messageObserved(m_lastObservation);
}

void TerminalRuntime::writeJournal(const QJsonObject &record)
{
    if (!m_journal.isOpen()) return;
    const auto line = QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
    if (m_journal.write(line) != line.size() || !m_journal.flush())
        qCritical() << "MQTT 消息/指令记录失败" << m_journal.errorString();
}

} // 命名空间 ly27
