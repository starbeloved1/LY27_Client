#pragma once

#include "TerminalConfig.hpp"
#include <QFile>
#include <QtLogging>
#include <memory>

// 日志工具。记录运行日志.log与通信记录.jsonl
namespace ly27 {

class RuntimeLogger {
public:
    RuntimeLogger() = default;
    ~RuntimeLogger();
    RuntimeLogger(const RuntimeLogger &) = delete;
    RuntimeLogger &operator=(const RuntimeLogger &) = delete;

    bool start(const QString &directory, LogLevel level, QString &error);
    // 先打开新文件再替换旧文件；失败时继续使用原日志。
    bool reconfigure(const QString &directory, LogLevel level, QString &error);
    void stop();
    QString filePath() const { return m_file ? m_file->fileName() : QString(); }

private:
    static void handleMessage(QtMsgType type, const QMessageLogContext &, const QString &message);
    std::unique_ptr<QFile> m_file;
    LogLevel m_level = LogLevel::Info;
    QtMessageHandler m_previous = nullptr;
    bool m_active = false;
};

} // 命名空间 ly27
