#include "RuntimeLogger.hpp"
#include "TerminalRuntime.hpp"
#include "TerminalConfig.hpp"
#include "MessageCodec.hpp"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
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
} // 命名空间

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ly27_cli");
    QCoreApplication::setApplicationVersion(LY27_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("LY27 自定义终端后端，使用 --mqtt 启用通信");
    const auto helpOption = parser.addHelpOption();
    const auto versionOption = parser.addVersionOption();
    const QCommandLineOption configOption({"c", "config"}, "Configuration JSON file.", "path",
        QDir(QCoreApplication::applicationDirPath()).filePath("config/terminal.example.json"));
    const QCommandLineOption checkOption("check-config", "Validate configuration and exit; do not create logs.");
    const QCommandLineOption statusOption("status", "Start backend, print JSON status and exit.");
    const QCommandLineOption runForOption("run-for", "Stop automatically after the given milliseconds.", "milliseconds");
    const QCommandLineOption mqttOption("mqtt", "启用 MQTT 连接和全部入站 Topic 订阅。");
    const QCommandLineOption videoOption("video", "启用 UDP 图传接收与 H.265 解码，可独立于 MQTT 使用。");
    const QCommandLineOption saveFrameOption("save-frame", "将第一张新鲜视频帧保存为 PNG，需要 --video 和 --run-for。", "path");
    const QCommandLineOption watchOption("watch", "将收到的协议消息输出为 JSON 行，需要 --mqtt。");
    const QCommandLineOption topicsOption("topics", "输出协议清单并退出，无需配置和网络。");
    const QCommandLineOption finalStatusOption("status-at-exit", "事件循环退出后输出最终状态。");
    const QCommandLineOption commandOption("command-file", "从 JSON 文件读取一条赛事操作，需要 --mqtt 和 --run-for。", "path");
    const QCommandLineOption delayOption("command-delay", "MQTT 就绪后等待指定毫秒再提交操作。", "milliseconds", "0");
    parser.addOptions({configOption, checkOption, statusOption, runForOption,
                       mqttOption, videoOption, saveFrameOption, watchOption, topicsOption, finalStatusOption, commandOption, delayOption});
    if (!parser.parse(app.arguments())) return fail(parser.errorText());
    if (parser.isSet(helpOption) || parser.isSet("help-all")) parser.showHelp(0);
    if (parser.isSet(versionOption)) parser.showVersion();
    if (parser.isSet(commandOption) && (!parser.isSet(mqttOption) || !parser.isSet(runForOption) || parser.isSet(watchOption)))
        return fail("--command-file 需要 --mqtt 和 --run-for，不能与 --watch 组合");
    if (parser.isSet(delayOption) && !parser.isSet(commandOption)) return fail("--command-delay 需要 --command-file");
    if (!parser.positionalArguments().isEmpty()) return fail("Unexpected positional arguments");
    const int modes = int(parser.isSet(checkOption)) + int(parser.isSet(statusOption))
                      + int(parser.isSet(runForOption)) + int(parser.isSet(topicsOption));
    if (modes > 1) return fail("--check-config、--status、--run-for、--topics 互斥");
    if (parser.isSet(watchOption) && !parser.isSet(mqttOption)) return fail("--watch 需要同时指定 --mqtt");
    if (parser.isSet(mqttOption) && (parser.isSet(statusOption) || parser.isSet(checkOption) || parser.isSet(topicsOption)))
        return fail("--mqtt 需要运行事件循环；查看最终状态请使用 --status-at-exit");
    if (parser.isSet(finalStatusOption) && (parser.isSet(statusOption) || parser.isSet(checkOption) || parser.isSet(topicsOption)))
        return fail("--status-at-exit 需要运行事件循环，不能与立即退出模式组合");
    if (parser.isSet(videoOption) && (parser.isSet(statusOption) || parser.isSet(checkOption) || parser.isSet(topicsOption)))
        return fail("--video 需要运行事件循环；查看最终状态请使用 --status-at-exit");
    if (parser.isSet(saveFrameOption) && (!parser.isSet(videoOption) || !parser.isSet(runForOption)
        || parser.value(saveFrameOption).isEmpty()))
        return fail("--save-frame 需要 --video、--run-for 和非空输出路径");
    if (parser.isSet(topicsOption)) { printJson(ly27::MessageCodec::manifest()); return 0; }
    int duration = 0;
    if (parser.isSet(runForOption)) {
        bool ok = false;
        const auto parsed = parser.value(runForOption).toLongLong(&ok);
        if (!ok || parsed < 1 || parsed > std::numeric_limits<int>::max())
            return fail("--run-for must be an integer in [1, 2147483647] milliseconds");
        duration = static_cast<int>(parsed);
    }

    std::optional<ly27::CommandRequest> command;
    int commandDelay = 0;
    if (parser.isSet(commandOption)) {
        bool ok = false;
        const auto delay = parser.value(delayOption).toLongLong(&ok);
        if (!ok || delay < 0 || delay >= duration) return fail("--command-delay 必须是小于运行时长的非负整数");
        commandDelay = static_cast<int>(delay);
        QFile file(parser.value(commandOption));
        if (!file.open(QIODevice::ReadOnly)) return fail("无法读取指令文件：" + file.errorString());
        if (file.size() > 65536) return fail("指令文件超过 64 KiB");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) return fail("指令文件必须是有效的 JSON 对象");
        const auto parsed = ly27::parseCommandRequest(document.object());
        if (!parsed.request) return fail(parsed.error);
        command = parsed.request;
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
    bool frameSaved = false, frameExportFailed = false;
    if (parser.isSet(saveFrameOption)) {
        QObject::connect(&runtime, &ly27::TerminalRuntime::videoFrameAvailable, &app, [&] {
            if (frameSaved || frameExportFailed) return;
            const auto frame = runtime.video().latestFrame();
            if (!frame) return;
            QSaveFile output(parser.value(saveFrameOption));
            if (!output.open(QIODevice::WriteOnly) || !frame->image.save(&output, "PNG") || !output.commit()) {
                qCritical().noquote() << "视频帧导出失败：" + output.errorString();
                frameExportFailed = true;
                app.exit(6);
                return;
            }
            frameSaved = true;
            qInfo().noquote() << "video.frame_saved path=" + parser.value(saveFrameOption);
        });
    }
    bool commandScheduled = false;
    bool commandAttempted = false;
    bool commandFailed = false;
    QString commandId;
    if (command) {
        QObject::connect(&runtime, &ly27::TerminalRuntime::commandObserved, &app, [](const QJsonObject &event) {
            QTextStream output(stdout);
            output << QJsonDocument(event).toJson(QJsonDocument::Compact) << '\n'; output.flush();
        });
        QObject::connect(&runtime.commands(), &ly27::CommandDispatcher::ready, &app, [&]() {
            if (commandScheduled) return;
            commandScheduled = true;
            QTimer::singleShot(commandDelay, &app, [&]() {
                commandAttempted = true;
                const auto submission = runtime.commands().dispatch(*command);
                commandFailed = !submission.accepted;
                commandId = submission.id;
                if (commandFailed) app.exit(5);
            });
        });
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &runtime, &ly27::TerminalRuntime::stop);
    if (parser.isSet(watchOption)) {
        QObject::connect(&runtime, &ly27::TerminalRuntime::messageObserved, &app, [](const QJsonObject &value) {
            QTextStream output(stdout);
            output << QJsonDocument(value).toJson(QJsonDocument::Compact) << '\n';
            output.flush();
        });
    }
    if (!runtime.start(parser.isSet(mqttOption), parser.isSet(videoOption))) return fail("后端启动失败，详见日志", 4);
    if (parser.isSet(statusOption)) {
        auto status = runtime.snapshot();
        status.insert("log_file", logger.filePath());
        printJson(status);
        runtime.stop();
        return 0;
    }

    // 信号处理函数只设置原子标记，所有 Qt 操作均在事件循环中执行。
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
    if (parser.isSet(finalStatusOption)) printJson(runtime.snapshot());
    if (previousInterrupt != SIG_ERR) std::signal(SIGINT, previousInterrupt);
    if (previousTerminate != SIG_ERR) std::signal(SIGTERM, previousTerminate);
    if (parser.isSet(saveFrameOption) && !frameSaved)
        return fail(frameExportFailed ? "视频帧导出失败" : "运行时限内未收到可导出的新鲜视频帧", 6);
    if (command && !commandAttempted) return fail("运行时限内未能提交操作，没有缓存待发送指令", 5);
    if (command && !commandFailed) {
        bool delivered = false;
        for (const auto &record : runtime.commands().snapshot().value("records").toArray()) {
            const auto object = record.toObject();
            if (object.value("id") == commandId && object.value("transport") == "delivered") delivered = true;
        }
        if (!delivered) return fail("未获得该操作的 MQTT 传输确认，服务器执行情况未知", 5);
    }
    return commandFailed ? 5 : code;
}
