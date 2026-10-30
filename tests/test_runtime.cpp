#include "app/TerminalRuntime.h"

#include <QSignalSpy>
#include <QtTest>

class RuntimeTest : public QObject {
    Q_OBJECT
private slots:
    void lifecycleAndSessionIsolation()
    {
        ly27::TerminalConfig config;
        config.identity.stationId = "test-station";
        config.identity.robotId = 3;
        ly27::TerminalRuntime runtime(config);
        QSignalSpy started(&runtime, &ly27::TerminalRuntime::started);
        QSignalSpy stopped(&runtime, &ly27::TerminalRuntime::stopped);
        QVERIFY(!runtime.isRunning());
        QVERIFY(runtime.snapshot()["session_id"].toString().isEmpty());
        runtime.stop();
        QCOMPARE(stopped.count(), 0);
        runtime.start();
        const auto session = runtime.snapshot()["session_id"].toString();
        QVERIFY(!session.isEmpty());
        runtime.start();
        QCOMPARE(started.count(), 1);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), session);
        runtime.stop();
        runtime.stop();
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(runtime.snapshot()["state"].toString(), QString("stopped"));
        runtime.start();
        QVERIFY(runtime.snapshot()["session_id"].toString() != session);
        QCOMPARE(started.count(), 2);
    }

    void doesNotClaimUnimplementedModulesAreConnected()
    {
        ly27::TerminalRuntime runtime({});
        runtime.start();
        const auto modules = runtime.snapshot()["modules"].toObject();
        for (const auto &key : {"mqtt", "video", "commands"})
            QCOMPARE(modules[key].toString(), QString("not_implemented"));
    }
};

QTEST_GUILESS_MAIN(RuntimeTest)
#include "test_runtime.moc"
