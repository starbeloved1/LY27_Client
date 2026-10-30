#pragma once

#include "config/TerminalConfig.h"
#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>

namespace ly27 {

// Owns application lifetime. Later steps attach transport/state/commands here.
// Neither CLI nor future QML owns backend services.
class TerminalRuntime : public QObject {
    Q_OBJECT
public:
    explicit TerminalRuntime(TerminalConfig config, QObject *parent = nullptr);
    ~TerminalRuntime() override;

    void start();
    void stop();
    bool isRunning() const { return m_running; }
    QJsonObject snapshot() const;

signals:
    void started();
    void stopped();

private:
    const TerminalConfig m_config;
    bool m_running = false;
    QString m_sessionId;
    QElapsedTimer m_uptime;
};

} // namespace ly27
