#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "TerminalRuntime.hpp"
#include "RuntimeLogger.hpp"

#include <QGuiApplication>
#include <QDebug>
#include <QCommandLineParser>
#include <QDir>
#include <QUuid>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("LY27 Client"));
    QCoreApplication::setOrganizationName(QStringLiteral("LY27"));
    QCoreApplication::setApplicationVersion(LY27_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("LY27 战术地图；使用 --mqtt 连接数据源。"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("mode", "cao：操作手（默认）；zhi：指挥手。", "[cao|zhi]");
    const QCommandLineOption configOption({"c", "config"}, "配置文件。", "path",
        QDir(QCoreApplication::applicationDirPath()).filePath("config/terminal.example.json"));
    const QCommandLineOption mqttOption("mqtt", "启用配置中的 MQTT 连接。");
    const QCommandLineOption redTeamOption("red-team", "本地设置红方队名。", "name", "红方队伍");
    const QCommandLineOption blueTeamOption("blue-team", "本地设置蓝方队名。", "name", "蓝方队伍");
    parser.addOptions({configOption, mqttOption, redTeamOption, blueTeamOption});
    parser.process(application);
    const auto arguments = parser.positionalArguments();
    if (arguments.size() > 1 || (!arguments.isEmpty() && arguments.first() != "cao" && arguments.first() != "zhi"))
        parser.showHelp(2);
    const bool commander = arguments.value(0, "cao") == "zhi";

    const auto loaded = ly27::loadConfig(parser.value(configOption));
    if (!loaded.config) { qCritical().noquote() << loaded.error; return 2; }
    auto config = *loaded.config;
    if (commander) {
        config.identity.role = ly27::Role::Commander;
        config.identity.robotId.reset();
        config.identity.stationId = ly27::teamName(config.identity.team) + "-commander";
        // 本地多窗口联调避免与操作手共用 Client ID 被 Broker 踢下线；不写回配置。
        // 现场 MQTT 身份仍需显式配置，不能用这里的测试名称推断官方指挥手 ID。
        if (config.network.mqttHost == "127.0.0.1" || config.network.mqttHost == "localhost")
            config.network.mqttClientId = "ly27-zhi-" + QUuid::createUuid().toString(QUuid::Id128);
    } else if (!config.identity.robotId || config.identity.role == ly27::Role::Commander) {
        qCritical() << "cao 模式需要配置绑定的操作手机器人身份。"; return 2;
    }
    ly27::RuntimeLogger logger;
    QString error;
    if (!logger.start(config.logDirectory, config.logLevel, error)) {
        qCritical().noquote() << error;
        return 3;
    }
    ly27::TerminalRuntime runtime(config, nullptr, &logger);
    if (!runtime.start(parser.isSet(mqttOption), false)) return 4;
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &runtime, &ly27::TerminalRuntime::stop);
    ly27::TerminalViewModel model(runtime.stateStore(), config.identity,
                                 parser.value(redTeamOption), parser.value(blueTeamOption));
    model.tactics()->setCommandDispatcher(&runtime.commands());
    ly27::MapWindow window(&model);
    window.setTitle(commander ? QStringLiteral("LY27 Client · 指挥手") : QStringLiteral("LY27 Client · 操作手"));
    if (window.status() == QQuickView::Error) {
        qCritical() << "无法加载场地底图界面。";
        return 1;
    }

    window.show();
    return application.exec();
}
