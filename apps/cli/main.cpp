#include "app/RuntimeLogger.h"
#include "app/TerminalRuntime.h"
#include "config/TerminalConfig.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>
#include <csignal>
#include <limits>

namespace {
volatile std::sig_atomic_t exitSignal = 0;
void requestExit(int signal) { exitSignal = signal; }

void printJson(const QJsonObject &value)
{
    QTextStream(stdout) << QJsonDocument(value).toJson(QJsonDocument::Indented);
}

int fail(const QString &message, int code = 2)
{
    QTextStream(stderr) << "ly27_cli: " << message << '\n';
    return code;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ly27_cli");
    QCoreApplication::setApplicationVersion(LY27_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("LY27 terminal backend — phase 1 (no network connections)");
    const auto helpOption = parser.addHelpOption();
    const auto versionOption = parser.addVersionOption();
    const QCommandLineOption configOption({"c", "config"}, "Configuration JSON file.", "path",
        QDir(QCoreApplication::applicationDirPath()).filePath("config/terminal.example.json"));
    const QCommandLineOption checkOption("check-config", "Validate configuration and exit; do not create logs.");
    const QCommandLineOption statusOption("status", "Start backend, print JSON status and exit.");
    const QCommandLineOption runForOption("run-for", "Stop automatically after the given milliseconds.", "milliseconds");
    parser.addOptions({configOption, checkOption, statusOption, runForOption});
    if (!parser.parse(app.arguments())) return fail(parser.errorText());
    if (parser.isSet(helpOption)) parser.showHelp(0);
    if (parser.isSet(versionOption)) parser.showVersion();
    if (!parser.positionalArguments().isEmpty()) return fail("Unexpected positional arguments");
    const int modes = int(parser.isSet(checkOption)) + int(parser.isSet(statusOption))
                      + int(parser.isSet(runForOption));
    if (modes > 1) return fail("--check-config, --status and --run-for are mutually exclusive");
    int duration = 0;
    if (parser.isSet(runForOption)) {
        bool ok = false;
        const auto parsed = parser.value(runForOption).toLongLong(&ok);
        if (!ok || parsed < 1 || parsed > std::numeric_limits<int>::max())
            return fail("--run-for must be an integer in [1, 2147483647] milliseconds");
        duration = static_cast<int>(parsed);
    }

    const auto result = ly27::loadConfig(parser.value(configOption));
    if (!result.config) return fail(result.error);
    if (parser.isSet(checkOption)) {
        printJson({{"valid", true}, {"config", result.config->toJson()}});
        return 0;
    }

    ly27::RuntimeLogger logger;
    QString error;
    if (!logger.start(result.config->logDirectory, result.config->logLevel, error))
        return fail(error, 3);
    qInfo().noquote() << "config.loaded path=" + result.config->sourcePath;
    qInfo().noquote() << "log.opened path=" + logger.filePath();
    ly27::TerminalRuntime runtime(*result.config);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &runtime, &ly27::TerminalRuntime::stop);
    runtime.start();
    if (parser.isSet(statusOption)) {
        auto status = runtime.snapshot();
        status.insert("log_file", logger.filePath());
        printJson(status);
        runtime.stop();
        return 0;
    }

    // Signal handlers only set sig_atomic_t; all Qt work happens in the event loop.
    const auto previousInterrupt = std::signal(SIGINT, requestExit);
    const auto previousTerminate = std::signal(SIGTERM, requestExit);
    QTimer signalPollTimer;
    signalPollTimer.setInterval(50);
    QObject::connect(&signalPollTimer, &QTimer::timeout, &app, [&app]() {
        if (exitSignal) {
            qInfo() << "runtime.exit_requested signal=" << int(exitSignal);
            app.quit();
        }
    });
    signalPollTimer.start();
    qInfo() << "runtime.ready";
    if (duration > 0) QTimer::singleShot(duration, &app, &QCoreApplication::quit);
    const int code = app.exec();
    if (previousInterrupt != SIG_ERR) std::signal(SIGINT, previousInterrupt);
    if (previousTerminate != SIG_ERR) std::signal(SIGTERM, previousTerminate);
    return code;
}
