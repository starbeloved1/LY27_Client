#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "TerminalRuntime.hpp"
#include "RuntimeLogger.hpp"

#include <QGuiApplication>
#include <QDebug>
#include <QCommandLineParser>
#include <QDir>

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
    const QCommandLineOption configOption({"c", "config"}, "配置文件。", "path",
        QDir(QCoreApplication::applicationDirPath()).filePath("config/terminal.example.json"));
    const QCommandLineOption mqttOption("mqtt", "启用配置中的 MQTT 连接。");
    const QCommandLineOption redTeamOption("red-team", "本地设置红方队名。", "name", "红方队伍");
    const QCommandLineOption blueTeamOption("blue-team", "本地设置蓝方队名。", "name", "蓝方队伍");
    parser.addOptions({configOption, mqttOption, redTeamOption, blueTeamOption});
    parser.process(application);
    if (!parser.positionalArguments().isEmpty()) parser.showHelp(2);

    const auto loaded = ly27::loadConfig(parser.value(configOption));
    if (!loaded.config) { qCritical().noquote() << loaded.error; return 2; }
    ly27::RuntimeLogger logger;
    QString error;
    if (!logger.start(loaded.config->logDirectory, loaded.config->logLevel, error)) {
        qCritical().noquote() << error;
        return 3;
    }
    ly27::TerminalRuntime runtime(*loaded.config, nullptr, &logger);
    if (!runtime.start(parser.isSet(mqttOption), false)) return 4;
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &runtime, &ly27::TerminalRuntime::stop);
    ly27::TerminalViewModel model(runtime.stateStore(), loaded.config->identity,
                                 parser.value(redTeamOption), parser.value(blueTeamOption));
    ly27::MapWindow window(&model);
    if (window.status() == QQuickView::Error) {
        qCritical() << "无法加载场地底图界面。";
        return 1;
    }

    window.show();
    return application.exec();
}
