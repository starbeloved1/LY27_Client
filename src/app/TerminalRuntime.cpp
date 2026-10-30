#include "TerminalRuntime.h"

#include <QDebug>
#include <QUuid>
#include <utility>

namespace ly27 {

TerminalRuntime::TerminalRuntime(TerminalConfig config, QObject *parent)
    : QObject(parent), m_config(std::move(config))
{
}

TerminalRuntime::~TerminalRuntime() { stop(); }

void TerminalRuntime::start()
{
    if (m_running) return;
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_uptime.start();
    m_running = true;
    qInfo().noquote() << "runtime.started session=" + m_sessionId
                     << "station=" + m_config.identity.stationId
                     << "role=" + roleName(m_config.identity.role)
                     << "team=" + teamName(m_config.identity.team);
    emit started();
}

void TerminalRuntime::stop()
{
    if (!m_running) return;
    m_running = false;
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
        {"modules", QJsonObject{{"mqtt", "not_implemented"},
                               {"video", "not_implemented"},
                               {"commands", "not_implemented"}}}
    };
}

} // namespace ly27
