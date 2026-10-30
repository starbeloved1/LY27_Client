#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_UNIX
#include <csignal>
#include <unistd.h>
#endif

class CliTest : public QObject {
    Q_OBJECT
private:
    QString writeConfig(const QString &directory, const QString &level = "info") const
    {
        QFile example(LY27_EXAMPLE_CONFIG);
        if (!example.open(QIODevice::ReadOnly)) return {};
        auto object = QJsonDocument::fromJson(example.readAll()).object();
        object["logging"] = QJsonObject{{"directory", "logs"}, {"level", level}};
        const auto path = QDir(directory).filePath("terminal.json");
        QFile output(path);
        if (!output.open(QIODevice::WriteOnly)) return {};
        if (output.write(QJsonDocument(object).toJson()) < 0) return {};
        return path;
    }

    void configureProcess(QProcess &process, const QStringList &arguments) const
    {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove("DISPLAY");
        environment.remove("WAYLAND_DISPLAY");
        process.setProcessEnvironment(environment);
        process.setProgram(LY27_CLI_PATH);
        process.setArguments(arguments);
    }

private slots:
    void validationHasNoLoggingSideEffects()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto config = writeConfig(directory.path());
        QVERIFY(!config.isEmpty());
        QProcess process;
        configureProcess(process, {"--config", config, "--check-config"});
        process.start();
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(QJsonDocument::fromJson(process.readAllStandardOutput()).object()["valid"].toBool());
        QVERIFY(!QDir(directory.filePath("logs")).exists());
    }

    void statusWorksFromUnrelatedDirectoryWithoutDisplay()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QProcess process;
        configureProcess(process, {"--status"});
        process.setWorkingDirectory(directory.path());
        process.start();
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 0);
        const auto status = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
        QCOMPARE(status["state"].toString(), QString("running"));
        QVERIFY(!status["config"].toObject()["source_path"].toString().isEmpty());
        const auto path = status["log_file"].toString();
        QFile log(path);
        QVERIFY(log.open(QIODevice::ReadOnly));
        const auto content = log.readAll();
        QVERIFY(content.contains("runtime.started"));
        QVERIFY(content.contains("runtime.stopped"));
    }

    void timedExitAndLogLevel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto config = writeConfig(directory.path(), "warning");
        QProcess process;
        configureProcess(process, {"--config", config, "--run-for", "30"});
        process.start();
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 0);
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY(process.readAllStandardError().isEmpty());
        const auto files = QDir(directory.filePath("logs")).entryList({"*.log"}, QDir::Files);
        QCOMPARE(files.size(), 1);
        QFile log(QDir(directory.filePath("logs")).filePath(files.front()));
        QVERIFY(log.open(QIODevice::ReadOnly));
        QVERIFY(log.readAll().isEmpty());
    }

    void reportsInvalidConfigAndArguments()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto config = writeConfig(directory.path());
        const QList<QStringList> badArguments = {
            {"--config", directory.filePath("missing.json")},
            {"--run-for", "0"}, {"--run-for", "no"}, {"--run-for", "2147483648"},
            {"--status", "--check-config"}, {"unexpected"},
            {"--unknown-option"}, {"--config"}
        };
        for (const auto &arguments : badArguments) {
            QProcess process;
            configureProcess(process, arguments);
            process.start();
            QVERIFY(process.waitForFinished(5000));
            QCOMPARE(process.exitCode(), 2);
            QVERIFY(!process.readAllStandardError().isEmpty());
        }
        QVERIFY(!QDir(directory.filePath("logs")).exists());
    }

    void helpAndVersionDoNotRequireConfig()
    {
        for (const auto &option : {"--help", "--version"}) {
            QProcess process;
            configureProcess(process, {"--config", "/missing/terminal.json", option});
            process.start();
            QVERIFY(process.waitForFinished(5000));
            QCOMPARE(process.exitCode(), 0);
            QVERIFY(!process.readAllStandardOutput().isEmpty());
        }
    }

    void reportsUnwritableLogLocation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto config = writeConfig(directory.path());
        QFile blocker(directory.filePath("logs"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QProcess process;
        configureProcess(process, {"--config", config, "--status"});
        process.start();
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 3);
        QVERIFY(process.readAllStandardError().contains("log directory"));
    }

    void gracefulTermination_data()
    {
        QTest::addColumn<int>("signalNumber");
#ifdef Q_OS_UNIX
        QTest::newRow("SIGINT") << int(SIGINT);
        QTest::newRow("SIGTERM") << int(SIGTERM);
#else
        QTest::newRow("unsupported") << 0;
#endif
    }

    void gracefulTermination()
    {
#ifndef Q_OS_UNIX
        QSKIP("QProcess::terminate uses different semantics outside Unix");
#else
        QFETCH(int, signalNumber);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto config = writeConfig(directory.path());
        QProcess process;
        configureProcess(process, {"--config", config});
        process.start();
        QVERIFY(process.waitForStarted(5000));
        QByteArray output;
        QElapsedTimer deadline;
        deadline.start();
        while (!output.contains("runtime.ready") && deadline.elapsed() < 5000) {
            process.waitForReadyRead(100);
            output += process.readAllStandardError();
        }
        QVERIFY2(output.contains("runtime.ready"), output.constData());
        QCOMPARE(::kill(static_cast<pid_t>(process.processId()), signalNumber), 0);
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        output += process.readAllStandardError();
        QVERIFY(output.contains("runtime.exit_requested"));
        QVERIFY(output.contains("runtime.stopped"));
#endif
    }
};

QTEST_GUILESS_MAIN(CliTest)
#include "test_cli.moc"
