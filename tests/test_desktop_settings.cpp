#include "DesktopSession.hpp"
#include "MapWindow.hpp"
#include "MqttTransport.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickItem>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QtTest>

namespace {
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
QByteArray read(const QString &path)
{
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
}

class DesktopSettingsTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QProcess broker;
    quint16 port = 0;
    ly27::TerminalConfig config(const QString &name)
    {
        ly27::TerminalConfig c;
        c.sourcePath = directory.filePath(name + ".json");
        c.identity = {ly27::Role::Infantry, ly27::Team::Red, "red-infantry-3", 3};
        c.network.mqttHost = "127.0.0.1"; c.network.mqttPort = port;
        c.network.mqttClientId = "settings-" + name;
        c.logDirectory = directory.filePath(name + "-logs");
        return c;
    }
    bool publish(ly27::MqttTransport &publisher, const google::protobuf::Message &message)
    {
        return publisher.publish(QString::fromStdString(message.GetDescriptor()->name()),
                                 QByteArray::fromStdString(message.SerializeAsString()));
    }
private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QTcpServer reservation; QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        port = reservation.serverPort(); reservation.close();
        QFile file(directory.filePath("broker.conf")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QString("listener %1 127.0.0.1\nallow_anonymous true\npersistence false\n").arg(port).toUtf8()); file.close();
        broker.start(LY27_BROKER_PATH, {"-c", file.fileName()}); QVERIFY(broker.waitForStarted(3000));
        QTRY_VERIFY_WITH_TIMEOUT(([&] { QTcpSocket s; s.connectToHost(QHostAddress::LocalHost, port); return s.waitForConnected(30); })(), 3000);
    }
    void cleanupTestCase()
    {
        broker.terminate();
        if (!broker.waitForFinished(3000)) { broker.kill(); broker.waitForFinished(); }
    }
    void actualSettingsClicksSwitchEverySeatAndSide()
    {
        auto c = config("offline"); QString error;
        QVERIFY(ly27::saveConfig(c, c.sourcePath, error)); const auto originalFile = read(c.sourcePath);
        ly27::TerminalRuntime runtime(c); QVERIFY(runtime.start());
        ly27::DesktopSession session(runtime, "红方测试队", "蓝方测试队");
        ly27::MapWindow window(session.viewModel(), nullptr, &session);
        QCOMPARE(window.status(), QQuickView::Ready); window.show(); QTest::qWait(40);
        auto *root = window.rootObject(); auto *settings = item(root, "terminalSettings"); QVERIFY(settings);
        auto click = [&](const QString &name) {
            auto *button = item(root, name);
            if (!button) return false;
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
            QCoreApplication::processEvents(); return true;
        };
        QVERIFY(click("settingsButton")); QVERIFY(settings->property("opened").toBool());
        for (const auto &choice : QList<QPair<QString, QString>>{{"roleHeavy", "heavy"}, {"roleCommander", "commander"},
                     {"roleInfantry4", "infantry4"}, {"roleInfantry3", "infantry3"}}) {
            QVERIFY(click(choice.first)); QCOMPARE(session.seat(), choice.second);
            for (const QString &team : {QString("blue"), QString("red")}) {
                QPointer<ly27::TerminalViewModel> previous = session.viewModel();
                QVERIFY(click(team == "blue" ? "teamBlue" : "teamRed"));
                QCOMPARE(session.team(), team);
                QVERIFY(session.viewModel() != previous);
                QCOMPARE(root->property("viewModel").value<QObject *>(), session.viewModel());
                QCOMPARE(session.viewModel()->blueView(), team == "blue");
                QCOMPARE(session.viewModel()->tactics()->commander(), choice.second == "commander");
                QCOMPARE(runtime.config().network.mqttClientId, c.network.mqttClientId);
                QVERIFY(settings->property("opened").toBool());
                if (choice.second == "commander") QVERIFY(!runtime.stateStore().self());
                else {
                    const int number = choice.second == "heavy" ? 1 : choice.second == "infantry4" ? 4 : 3;
                    QCOMPARE(runtime.config().identity.robotId, std::optional<int>(number + (team == "blue" ? 100 : 0)));
                }
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QVERIFY(previous.isNull());
            }
        }
        QCOMPARE(read(c.sourcePath), originalFile);
        const auto before = runtime.snapshot()["session_id"].toString();
        QVERIFY(session.selectIdentity(session.seat(), session.team())); QCOMPARE(runtime.snapshot()["session_id"].toString(), before);
        QVERIFY(!session.selectIdentity("unknown", "red")); QCOMPARE(runtime.snapshot()["session_id"].toString(), before);
        QVERIFY(!session.error().isEmpty()); QVERIFY(session.selectIdentity(session.seat(), session.team()));
        const auto normalSize = window.size();
        QVERIFY(click("fullScreenOption"));
        QTRY_VERIFY(window.fullScreen());
        QCOMPARE(window.visibility(), QWindow::FullScreen);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), before);
        QVERIFY(click("fullScreenOption"));
        QTRY_VERIFY(!window.fullScreen());
        QTRY_COMPARE(window.size(), normalSize);
        QCOMPARE(runtime.snapshot()["session_id"].toString(), before);
        for (const QSize size : {QSize(1280, 800), QSize(640, 480)}) {
            window.resize(size); QTest::qWait(30);
            auto *panel = item(root, "terminalSettingsPanel");
            QVERIFY(QRectF(QPointF(), root->size()).contains(panel->mapRectToItem(root, QRectF(QPointF(), panel->size()))));
            QDir().mkpath("test-artifacts");
            QVERIFY(window.grabWindow().save(QString("test-artifacts/settings-%1.png").arg(size.width())));
        }
        QTest::keyClick(&window, Qt::Key_Escape); QVERIFY(!settings->property("opened").toBool());
        window.setSource(QUrl());
    }
    void mqttReconnectionClearsOldIdentityAndRemapsIncomingData()
    {
        auto c = config("mqtt"); ly27::TerminalRuntime runtime(c); QVERIFY(runtime.start(true));
        ly27::DesktopSession session(runtime, "红队", "蓝队");
        ly27::MqttTransport publisher; auto network = c.network; network.mqttClientId = "settings-publisher";
        QVERIFY(publisher.start(network, {}));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot()["mqtt"].toObject()["state"].toString(), QString("ready"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(publisher.snapshot()["state"].toString(), QString("ready"), 5000);
        ly27::wire::GameStatus game; game.set_current_stage(4); game.set_current_round(1);
        game.set_stage_countdown_sec(400); game.set_stage_elapsed_sec(20); game.set_is_paused(false);
        ly27::wire::GlobalUnitStatus units; units.set_base_health(5000); units.set_enemy_base_health(6000);
        ly27::wire::Event event; event.set_event_id(11);
        QVERIFY(publish(publisher, game)); QVERIFY(publish(publisher, units)); QVERIFY(publish(publisher, event));
        QTRY_COMPARE(session.viewModel()->match()["redBaseHp"].toInt(), 5000);
        QTRY_COMPARE(session.viewModel()->events()->rowCount(), 1);
        const auto oldSession = runtime.snapshot()["session_id"].toString();
        const auto *store = &runtime.stateStore(); const auto *commands = &runtime.commands();
        QVERIFY(session.selectIdentity("heavy", "blue"));
        QVERIFY(runtime.snapshot()["session_id"].toString() != oldSession);
        QCOMPARE(&runtime.stateStore(), store); QCOMPARE(&runtime.commands(), commands);
        QCOMPARE(session.viewModel()->events()->rowCount(), 0);
        QCOMPARE(session.viewModel()->match()["redBaseHp"].toInt(), -1);
        QCOMPARE(runtime.config().identity.robotId, std::optional<int>(101));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot()["mqtt"].toObject()["state"].toString(), QString("ready"), 5000);
        units.set_base_health(7000); units.set_enemy_base_health(2000);
        QVERIFY(publish(publisher, game)); QVERIFY(publish(publisher, units));
        QTRY_COMPARE(session.viewModel()->match()["redBaseHp"].toInt(), 2000);
        QCOMPARE(session.viewModel()->match()["blueBaseHp"].toInt(), 7000);
        runtime.stop(); publisher.stop();
    }
    void identitySwitchDoesNotReplaySettlement()
    {
        auto c = config("result"); ly27::TerminalRuntime runtime(c); QVERIFY(runtime.start(true));
        ly27::DesktopSession session(runtime, "红队", "蓝队");
        ly27::MapWindow window(session.viewModel(), nullptr, &session);
        QCOMPARE(window.status(), QQuickView::Ready); window.show();
        auto *overlay = item(window.rootObject(), "matchPhaseOverlay"); QVERIFY(overlay);
        ly27::MqttTransport publisher; auto network = c.network; network.mqttClientId = "result-publisher";
        QVERIFY(publisher.start(network, {}));
        QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot()["mqtt"].toObject()["state"].toString(), QString("ready"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(publisher.snapshot()["state"].toString(), QString("ready"), 5000);
        ly27::wire::GameStatus game; game.set_current_round(1); game.set_current_stage(4); game.set_is_paused(false);
        game.set_stage_countdown_sec(400); game.set_stage_elapsed_sec(20);
        QVERIFY(publish(publisher, game));
        QTRY_COMPARE(session.viewModel()->match()["timer"].toString(), QString("06:40"));
        game.set_current_stage(5); game.set_game_result(1); game.set_end_reason(1);
        game.set_stage_countdown_sec(0); game.set_stage_elapsed_sec(420);
        QVERIFY(publish(publisher, game));
        QTRY_COMPARE(session.viewModel()->phase()["kind"].toString(), QString("result"));
        QVERIFY(overlay->isVisible()); QVERIFY(session.viewModel()->phase()["animateVictory"].toBool());
        overlay->setProperty("dismissedToken", overlay->property("token")); QVERIFY(!overlay->isVisible());

        for (const auto &choice : QList<QPair<QString, QString>>{{"heavy", "red"}, {"commander", "blue"}, {"infantry4", "red"}}) {
            QVERIFY(session.selectIdentity(choice.first, choice.second));
            QVERIFY(!overlay->isVisible());
            QTRY_COMPARE_WITH_TIMEOUT(runtime.snapshot()["mqtt"].toObject()["state"].toString(), QString("ready"), 5000);
            // 先恢复无胜者结算，再补胜者，周期重复也不能弹出旧战报。
            game.clear_game_result(); QVERIFY(publish(publisher, game));
            QTRY_COMPARE(session.viewModel()->match()["timer"].toString(), QString("00:00"));
            QVERIFY(!overlay->isVisible());
            game.set_game_result(1); QVERIFY(publish(publisher, game));
            QTRY_VERIFY(runtime.stateStore().matchState().result.isFresh());
            QCOMPARE(session.viewModel()->phase()["kind"].toString(), QString("none"));
            QVERIFY(!overlay->isVisible());
            QVERIFY(publish(publisher, game)); QTest::qWait(50); QVERIFY(!overlay->isVisible());
        }
        // 下一局真实结束仍可正常播放，不将一次身份切换变成永久关闭胜利播报。
        game.set_current_round(2); game.set_current_stage(4); game.clear_game_result();
        game.set_stage_countdown_sec(320); game.set_stage_elapsed_sec(100);
        QVERIFY(publish(publisher, game));
        QTRY_COMPARE(session.viewModel()->match()["timer"].toString(), QString("05:20"));
        game.set_current_stage(5); game.set_game_result(2); game.set_stage_countdown_sec(0); game.set_stage_elapsed_sec(420);
        QVERIFY(publish(publisher, game));
        QTRY_COMPARE(session.viewModel()->phase()["kind"].toString(), QString("result"));
        QCOMPARE(session.viewModel()->phase()["art"].toString(), QString("blue"));
        QVERIFY(session.viewModel()->phase()["animateVictory"].toBool()); QVERIFY(overlay->isVisible());
        window.setSource(QUrl()); runtime.stop(); publisher.stop();
    }
    void executableConnectsWithoutModeOrMqttFlag()
    {
        auto c = config("startup"); QString error;
        QUdpSocket reservation; QVERIFY(reservation.bind(QHostAddress::LocalHost, 0));
        c.network.videoPort = reservation.localPort(); reservation.close();
        c.identity.role = ly27::Role::Commander; c.identity.robotId.reset(); c.identity.stationId = "red-commander";
        QVERIFY(ly27::saveConfig(c, c.sourcePath, error));
        QProcess client; client.setProcessChannelMode(QProcess::MergedChannels);
        client.start(LY27_DESKTOP_PATH, {"--config", c.sourcePath});
        QVERIFY(client.waitForStarted(3000));
        QByteArray output;
        QTRY_VERIFY_WITH_TIMEOUT(([&] { output += client.readAll(); return output.contains("mqtt.state=ready") || client.state() == QProcess::NotRunning; })(), 5000);
        QVERIFY2(output.contains("mqtt.state=ready"), output.constData());
        QVERIFY2(output.contains("video.state=waiting"), output.constData());
        QCOMPARE(client.state(), QProcess::Running);
        client.terminate(); QVERIFY(client.waitForFinished(3000));
        client.start(LY27_DESKTOP_PATH, {"--help"}); QVERIFY(client.waitForFinished(3000));
        const auto help = client.readAll(); QCOMPARE(client.exitCode(), 0);
        QVERIFY(!help.contains("--mqtt")); QVERIFY(!help.contains("cao")); QVERIFY(!help.contains("zhi"));
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv); DesktopSettingsTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_desktop_settings.moc"
