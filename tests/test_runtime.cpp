#include "TerminalRuntime.hpp"

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
        QCOMPARE(runtime.stateStore().topic("GameStatus").validity, QString("unknown"));
        runtime.stop();
        QCOMPARE(stopped.count(), 0);
        runtime.start();
        const auto session = runtime.snapshot()["session_id"].toString();
        QVERIFY(!session.isEmpty());
        QCOMPARE(runtime.snapshot()["business_state"].toObject()["session_id"].toString(), session);
        QVERIFY(!runtime.snapshot()["business_state"].toObject()["connected"].toBool());
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
        QCOMPARE(modules["mqtt"].toString(), QString("disabled"));
        QCOMPARE(modules["video"].toString(), QString("not_implemented"));
        QCOMPARE(modules["commands"].toString(), QString("offline"));
    }
};

QTEST_GUILESS_MAIN(RuntimeTest)
#include "test_runtime.moc"
