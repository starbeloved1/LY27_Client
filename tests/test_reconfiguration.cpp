#include "TerminalRuntime.hpp"
#include "RuntimeLogger.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QUdpSocket>
#include <QTemporaryDir>
#include <QtTest>

namespace {
ly27::TerminalConfig configFor(const QString &directory)
{
    ly27::TerminalConfig config;
    config.sourcePath = QDir(directory).filePath("terminal.json");
    config.identity.role = ly27::Role::Infantry;
    config.identity.team = ly27::Team::Red;
    config.identity.stationId = "red-infantry-3";
    config.identity.robotId = 3;
    config.network.mqttHost = "127.0.0.1";
    config.network.mqttClientId = "test-infantry";
    config.logDirectory = QDir(directory).filePath("logs");
    return config;
}
bool replace(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
}

class ReconfigurationTest : public QObject {
    Q_OBJECT
private slots:
    void identityChangeKeepsObjectsAndRevalidatesCommands()
    {
        QTemporaryDir directory;
        auto config = configFor(directory.path());
        ly27::TerminalRuntime runtime(config);
        const auto *store = &runtime.stateStore();
        const auto *commands = &runtime.commands();
        const auto *video = &runtime.video();
        QVERIFY(runtime.start());
        const auto session = runtime.snapshot()["session_id"].toString();
        QSignalSpy changed(&runtime, &ly27::TerminalRuntime::configurationChanged);
        ly27::TerminalIdentity identity;
        identity.role = ly27::Role::Commander;
        identity.team = ly27::Team::Blue;
        identity.stationId = "blue-commander";
        identity.robotId.reset();
        auto result = runtime.switchIdentity(identity, "explicit-commander-id");
        QVERIFY2(result.applied, qPrintable(result.error));
        QVERIFY(result.restarted);
        QCOMPARE(changed.count(), 1);
        QVERIFY(runtime.isRunning());
        QCOMPARE(&runtime.stateStore(), store);
        QCOMPARE(&runtime.commands(), commands);
        QCOMPARE(&runtime.video(), video);
        QCOMPARE(runtime.config().identity.team, ly27::Team::Blue);
        QCOMPARE(runtime.config().network.mqttClientId, QString("explicit-commander-id"));
        QCOMPARE(runtime.stateStore().roleState().role, ly27::Role::Commander);
        QVERIFY(!runtime.stateStore().self());
        QCOMPARE(runtime.stateStore().globalState().allyBase.team, ly27::Team::Blue);
        QVERIFY(runtime.snapshot()["session_id"].toString() != session);
        const auto rejected = runtime.commands().selectPerformance({ly27::ShooterPerformance::Burst, {}});
        QVERIFY(!rejected.accepted);
        QCOMPARE(rejected.code, QString("role_denied"));
        runtime.stop();
        identity.role = ly27::Role::Heavy;
        identity.robotId = 101;
        identity.stationId = "blue-heavy";
        result = runtime.switchIdentity(identity, "heavy");
        QVERIFY(result.applied);
        QVERIFY(!result.restarted);
        QVERIFY(!runtime.isRunning());
        QCOMPARE(runtime.stateStore().roleState().role, ly27::Role::Heavy);
        QCOMPARE(runtime.stateStore().self()->robotId, 101);
        QVERIFY(runtime.commands().snapshot()["records"].toArray().isEmpty());
        QVERIFY(runtime.start());
        QCOMPARE(runtime.stateStore().self()->robotId, 101);
    }

    void validationAndReloadFailuresPreserveSession()
    {
        QTemporaryDir directory;
        auto config = configFor(directory.path());
        QString error;
        QVERIFY(ly27::saveConfig(config, config.sourcePath, error));
        ly27::TerminalRuntime runtime(config);
        QVERIFY(runtime.start());
        const auto session = runtime.snapshot()["session_id"].toString();
        auto invalid = config; invalid.identity.robotId = 101;
        QVERIFY(!runtime.applyConfig(invalid).applied);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), session);
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(3));
        QVERIFY(replace(config.sourcePath, "{"));
        QVERIFY(!runtime.reloadConfig().applied);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), session);
        QVERIFY(!runtime.reloadConfig(directory.filePath("missing.json")).applied);
        QVERIFY(runtime.isRunning());
        QCOMPARE(runtime.snapshot()["session_id"].toString(), session);
        QCOMPARE(runtime.applyConfig(config).restarted, false);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), session);
        auto other = config; other.identity.robotId = 4;
        other.network.mqttClientId = "four";
        const auto otherPath = directory.filePath("other.json");
        QVERIFY(ly27::saveConfig(other, otherPath, error));
        QVERIFY(runtime.reloadConfig(otherPath).applied);
        QCOMPARE(runtime.config().sourcePath, otherPath);
        QCOMPARE(runtime.stateStore().self()->robotId, 4);
    }

    void videoStartupRollbackAndRollbackFailure()
    {
        QTemporaryDir directory;
        auto config = configFor(directory.path());
        QUdpSocket port, blocker;
        QVERIFY(port.bind(QHostAddress::LocalHost, 0));
        config.network.videoPort = port.localPort(); port.close();
        QVERIFY(blocker.bind(QHostAddress::LocalHost, 0));
        ly27::TerminalRuntime runtime(config);
        QVERIFY(runtime.start(false, true));
        const auto session = runtime.snapshot()["session_id"].toString();
        auto next = config;
        next.identity.robotId = 4;
        next.network.videoPort = blocker.localPort();
        auto result = runtime.applyConfig(next);
        QVERIFY(!result.applied);
        QVERIFY2(result.rolledBack, qPrintable(result.rollbackError));
        QVERIFY(runtime.isRunning());
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(3));
        QCOMPARE(runtime.video().snapshot()["state"].toString(), QString("waiting"));
        QVERIFY(runtime.snapshot()["session_id"].toString() != session);
        QVERIFY(!runtime.video().latestFrame());
        // 在旧端口释放时模拟其被其他进程占用，验证恢复失败会明确返回。
        QUdpSocket stolen;
        bool steal = false;
        connect(&runtime, &ly27::TerminalRuntime::videoStateChanged, this, [&](const QString &state) {
            if (state == "stopped" && !steal) {
                steal = stolen.bind(QHostAddress::LocalHost, config.network.videoPort, QUdpSocket::DontShareAddress);
            }
        });
        result = runtime.applyConfig(next);
        QVERIFY(!result.applied);
        QVERIFY(steal);
        QVERIFY(!result.rolledBack);
        QVERIFY(!result.rollbackError.isEmpty());
        QVERIFY(!runtime.isRunning());
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(3));
    }

    void loggerUpdateAndFailurePreservesOldLog()
    {
        QTemporaryDir directory;
        auto config = configFor(directory.path());
        QString error;
        ly27::RuntimeLogger logger;
        QVERIFY(logger.start(config.logDirectory, config.logLevel, error));
        ly27::TerminalRuntime runtime(config, nullptr, &logger);
        QVERIFY(runtime.start());
        const auto original = logger.filePath();
        auto next = config;
        next.logDirectory = directory.filePath("new-logs");
        next.logLevel = ly27::LogLevel::Warning;
        QVERIFY(runtime.applyConfig(next).applied);
        QVERIFY(logger.filePath() != original);
        QVERIFY(logger.filePath().startsWith(next.logDirectory));
        qInfo("filtered-test-message");
        qWarning("retained-test-message");
        QFile log(logger.filePath()); QVERIFY(log.open(QIODevice::ReadOnly));
        auto contents = log.readAll();
        QVERIFY(!contents.contains("filtered-test-message"));
        QVERIFY(contents.contains("retained-test-message"));
        const auto workingLog = logger.filePath();
        QFile blocker(directory.filePath("blocked")); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.close();
        next.logDirectory = blocker.fileName();
        next.identity.robotId = 4;
        const auto result = runtime.applyConfig(next);
        QVERIFY(!result.applied); QVERIFY(result.rolledBack);
        QVERIFY(runtime.isRunning());
        QCOMPARE(logger.filePath(), workingLog);
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(3));
        qWarning("still-logging-after-failure");
        log.close(); QVERIFY(log.open(QIODevice::ReadOnly));
        QVERIFY(log.readAll().contains("still-logging-after-failure"));
    }

    void watchedAtomicReplaceDeletionAndRecovery()
    {
        QTemporaryDir directory;
        auto config = configFor(directory.path());
        QString error;
        QVERIFY(ly27::saveConfig(config, config.sourcePath, error));
        ly27::TerminalRuntime runtime(config);
        QVERIFY(runtime.start());
        QVERIFY(runtime.setConfigWatching(true, error));
        QSignalSpy changed(&runtime, &ly27::TerminalRuntime::configurationChanged);
        QSignalSpy rejected(&runtime, &ly27::TerminalRuntime::configurationChangeFailed);
        auto next = config; next.identity.robotId = 4;
        QVERIFY(ly27::saveConfig(next, config.sourcePath, error));
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(4));
        const auto session = runtime.snapshot()["session_id"].toString();
        QVERIFY(replace(config.sourcePath, "{"));
        QTRY_VERIFY_WITH_TIMEOUT(!rejected.isEmpty(), 3000);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), session);
        QVERIFY(QFile::remove(config.sourcePath));
        QTest::qWait(250);
        QVERIFY(ly27::saveConfig(config, config.sourcePath, error));
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 2, 3000);
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(3));
        const auto unchanged = runtime.snapshot()["session_id"].toString();
        QVERIFY(ly27::saveConfig(config, config.sourcePath, error));
        QTest::qWait(300);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), unchanged);
        QVERIFY(runtime.setConfigWatching(false, error));
        QVERIFY(ly27::saveConfig(next, config.sourcePath, error));
        QTest::qWait(300);
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(3));
    }

    void cliWatchesIdentityAndLogging()
    {
        QTemporaryDir directory;
        auto config = configFor(directory.path());
        QString error;
        QVERIFY(ly27::saveConfig(config, config.sourcePath, error));
        QProcess cli;
        cli.start(LY27_CLI_PATH, {"--config", config.sourcePath, "--watch-config", "--run-for", "1600", "--status-at-exit"});
        QVERIFY(cli.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QDir(config.logDirectory).entryList({"*.log"}, QDir::Files).size() > 0, 1000);
        config.identity.robotId = 4;
        config.network.mqttClientId = "four";
        config.logDirectory = directory.filePath("new-logs");
        QVERIFY(ly27::saveConfig(config, config.sourcePath, error));
        QTRY_COMPARE_WITH_TIMEOUT(cli.state(), QProcess::NotRunning, 4000);
        QCOMPARE(cli.exitCode(), 0);
        const auto status = QJsonDocument::fromJson(cli.readAllStandardOutput()).object();
        QCOMPARE(status["config"].toObject()["identity"].toObject()["robot_id"].toInt(), 4);
        QCOMPARE(status["config"].toObject()["logging"].toObject()["directory"].toString(), config.logDirectory);
        QVERIFY(status["configuration"].toObject()["watching"].toBool());
        QVERIFY(QDir(config.logDirectory).entryList({"*.log"}, QDir::Files).size() > 0);
    }
};
QTEST_GUILESS_MAIN(ReconfigurationTest)
#include "test_reconfiguration.moc"
