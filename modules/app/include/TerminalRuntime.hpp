#pragma once

#include "TerminalConfig.hpp"
#include "MqttTransport.hpp"
#include "StateStore.hpp"
#include "CommandDispatcher.hpp"
#include "VideoReceiver.hpp"
#include "RuntimeLogger.hpp"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>

// 主程序，管理启动与停止，组织后端对象
namespace ly27 {

struct ConfigApplyResult {
    bool applied = false;
    bool restarted = false;
    bool rolledBack = false;
    QString error;
    QString rollbackError;
};

// 通信、状态和指令模块在这里接入
// cli和desktop均通过此对象使用后端
class TerminalRuntime : public QObject {
    Q_OBJECT
public:
    // logger 由应用持有，需要比运行对象活得更久；传入后同步应用日志目录和等级。
    explicit TerminalRuntime(TerminalConfig config, QObject *parent = nullptr, RuntimeLogger *logger = nullptr);
    ~TerminalRuntime() override;

    bool start(bool enableMqtt = false, bool enableVideo = false);
    void stop();
    const TerminalConfig &config() const { return m_config; }
    ConfigApplyResult applyConfig(const TerminalConfig &config);
    ConfigApplyResult reloadConfig(const QString &path = {});
    ConfigApplyResult switchIdentity(const TerminalIdentity &identity, const QString &mqttClientId);
    bool saveCurrentConfig(const QString &path, QString &error) const;
    bool setConfigWatching(bool enabled, QString &error);
    bool isConfigWatching() const { return m_watchingConfig; }
    bool isRunning() const { return m_running; }
    QJsonObject snapshot() const;
    const StateStore &stateStore() const { return m_state; }
    CommandDispatcher &commands() { return m_commands; }
    const VideoReceiver &video() const { return m_video; }

signals:
    void configurationChanged();
    void configurationChangeFailed(const QString &error, const QString &rollbackError);
    void started();
    void stopped();
    void messageObserved(const QJsonObject &observation);
    void stateChanged(const QString &topic);
    void videoFrameAvailable();
    void videoStateChanged(const QString &state);
    void commandObserved(const QJsonObject &event);

private:
    TerminalConfig m_config;
    RuntimeLogger *m_logger = nullptr;
    bool m_operating = false;
    bool m_videoEnabled = false;
    bool m_watchingConfig = false;
    QString m_lastConfigError;
    qint64 m_configAppliedUtcMs = 0;
    QByteArray m_watchedContents;
    QFileSystemWatcher m_configWatcher{this};
    QTimer m_configDebounce{this};
    bool m_running = false;
    QString m_sessionId;
    QElapsedTimer m_uptime;
    MqttTransport m_mqtt;
    StateStore m_state;
    CommandDispatcher m_commands;
    VideoReceiver m_video;
    QFile m_journal;
    bool m_mqttEnabled = false;
    qint64 m_received = 0;
    qint64 m_parsed = 0;
    qint64 m_rejected = 0;
    qint64 m_unresolved = 0;
    QJsonObject m_lastObservation;

    bool startBackend(bool enableMqtt, bool enableVideo, QString &error);
    void stopBackend();
    void installConfig(const TerminalConfig &config);
    ConfigApplyResult rejectConfig(const QString &error, const QString &rollbackError = {}, bool rolledBack = false);
    bool refreshConfigWatch(QString &error);
    void checkWatchedConfig();
    void receiveMessage(const QString &topic, const QByteArray &payload, qint64 receivedUtcMs,
                        qint64 receivedMonotonicNs, int qos, bool retained);
    void writeJournal(const QJsonObject &record);
};

} // 命名空间 ly27
