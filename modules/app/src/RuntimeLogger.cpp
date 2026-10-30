#include "RuntimeLogger.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <cstdio>

namespace ly27 {
namespace {
QMutex loggerMutex;
RuntimeLogger *activeLogger = nullptr;

int priority(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return 0;
    case QtInfoMsg: return 1;
    case QtWarningMsg: return 2;
    case QtCriticalMsg: return 3;
    case QtFatalMsg: return 4;
    }
    return 4;
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARNING";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "UNKNOWN";
}
} // 命名空间

RuntimeLogger::~RuntimeLogger() { stop(); }

bool RuntimeLogger::start(const QString &directory, LogLevel level, QString &error)
{
    QMutexLocker lock(&loggerMutex);
    if (activeLogger) {
        error = "A runtime logger is already active";
        return false;
    }
    if (!QDir().mkpath(directory)) {
        error = "Cannot create log directory: " + directory;
        return false;
    }
    const QString name = QStringLiteral("ly27_%1_%2.log")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz"))
        .arg(QCoreApplication::applicationPid());
    m_file.setFileName(QDir(directory).filePath(name));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
        error = "Cannot open log file: " + m_file.errorString();
        return false;
    }
    m_level = level;
    activeLogger = this;
    m_previous = qInstallMessageHandler(&RuntimeLogger::handleMessage);
    m_active = true;
    return true;
}

void RuntimeLogger::stop()
{
    QMutexLocker lock(&loggerMutex);
    if (!m_active) return;
    qInstallMessageHandler(m_previous);
    activeLogger = nullptr;
    m_file.flush();
    m_file.close();
    m_active = false;
}

void RuntimeLogger::handleMessage(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    QMutexLocker lock(&loggerMutex);
    if (!activeLogger || priority(type) < static_cast<int>(activeLogger->m_level)) return;
    const auto line = QStringLiteral("%1 [%2] %3\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
             QString::fromLatin1(levelName(type)), message).toUtf8();
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
    std::fflush(stderr);
    activeLogger->m_file.write(line);
    activeLogger->m_file.flush();
}

} // 命名空间 ly27
