#include "RuntimeLogger.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
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
    error.clear();
    if (!QDir().mkpath(directory)) {
        error = "Cannot create log directory: " + directory;
        return false;
    }
    const auto name = QStringLiteral("ly27_%1_%2_%3.log")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz"))
        .arg(QCoreApplication::applicationPid())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    auto file = std::make_unique<QFile>(QDir(directory).filePath(name));
    if (!file->open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
        error = "Cannot open log file: " + file->errorString();
        return false;
    }
    m_file = std::move(file);
    m_level = level;
    activeLogger = this;
    m_previous = qInstallMessageHandler(&RuntimeLogger::handleMessage);
    m_active = true;
    return true;
}

bool RuntimeLogger::reconfigure(const QString &directory, LogLevel level, QString &error)
{
    QMutexLocker lock(&loggerMutex);
    error.clear();
    if (activeLogger != this || !m_active) { error = "运行日志尚未启动"; return false; }
    if (QFileInfo(m_file->fileName()).absolutePath() == QDir(directory).absolutePath()) {
        m_level = level;
        return true;
    }
    if (!QDir().mkpath(directory)) { error = "无法创建新日志目录：" + directory; return false; }
    const auto name = QStringLiteral("ly27_%1_%2_%3.log")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz"))
        .arg(QCoreApplication::applicationPid())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    auto file = std::make_unique<QFile>(QDir(directory).filePath(name));
    if (!file->open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
        error = "无法打开新日志文件：" + file->errorString(); return false;
    }
    m_file->flush();
    m_file.swap(file);
    m_level = level;
    return true;
}

void RuntimeLogger::stop()
{
    QMutexLocker lock(&loggerMutex);
    if (!m_active) return;
    qInstallMessageHandler(m_previous);
    activeLogger = nullptr;
    m_file->flush();
    m_file->close();
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
    activeLogger->m_file->write(line);
    activeLogger->m_file->flush();
}

} // 命名空间 ly27
