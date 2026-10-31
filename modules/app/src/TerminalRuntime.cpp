#include "TerminalRuntime.hpp"
#include "MessageCodec.hpp"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QUuid>
#include <utility>

namespace ly27 {

TerminalRuntime::TerminalRuntime(TerminalConfig config, QObject *parent)
    : QObject(parent), m_config(std::move(config)), m_mqtt(this), m_state(m_config.identity, this),
      m_commands(m_config.identity, m_state, m_mqtt, this)
{
    connect(&m_mqtt, &MqttTransport::stateChanged, this, [this](const QString &state) {
        qInfo().noquote() << "mqtt.state=" + state;
        m_state.setConnected(state == "subscribing" || state == "ready");
    });
    connect(&m_mqtt, &MqttTransport::errorOccurred, this, [](const QString &error) {
        qWarning().noquote() << "mqtt.error=" + error;
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

bool TerminalRuntime::start(bool enableMqtt)
{
    if (m_running) return true;
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_mqttEnabled = enableMqtt;
    m_received = m_parsed = m_rejected = m_unresolved = 0;
    m_lastObservation = {};
    m_state.reset(m_sessionId);
    if (enableMqtt) {
        if (m_config.network.mqttClientId.isEmpty()) {
            qCritical() << "启用 MQTT 前必须填写 network.mqtt_client_id";
            return false;
        }
        if (!QDir().mkpath(m_config.logDirectory)) {
            qCritical() << "无法创建 MQTT 记录目录";
            return false;
        }
        m_journal.setFileName(QDir(m_config.logDirectory).filePath("mqtt_" + m_sessionId + ".jsonl"));
        if (!m_journal.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            qCritical() << "无法创建 MQTT 原始消息记录" << m_journal.errorString();
            return false;
        }
        if (!m_mqtt.start(m_config.network, MessageCodec::inboundTopics())) {
            m_journal.close();
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
    emit started();
    return true;
}

void TerminalRuntime::stop()
{
    if (!m_running) return;
    m_running = false;
    m_commands.stop();
    m_mqtt.stop();
    m_state.setConnected(false);
    m_journal.close();
    qInfo().noquote() << "runtime.stopped session=" + m_sessionId
                     << "uptime_ms=" + QString::number(m_uptime.elapsed());
    m_uptime.invalidate();
    emit stopped();
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
        {"modules", QJsonObject{{"mqtt", m_mqttEnabled ? m_mqtt.snapshot().value("state") : QJsonValue("disabled")},
                               {"video", "not_implemented"},
                               {"commands", m_commands.snapshot().value("state")}}},
        {"mqtt", m_mqtt.snapshot()},
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
