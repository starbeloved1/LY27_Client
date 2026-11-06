#include "MapWindow.hpp"
#include "TerminalRuntime.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickItem>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

namespace {
ly27::TerminalIdentity identity(bool commander = false, int robot = 3)
{
    ly27::TerminalIdentity value;
    value.role = commander ? ly27::Role::Commander : ly27::Role::Infantry;
    value.team = robot >= 100 ? ly27::Team::Blue : ly27::Team::Red;
    value.stationId = commander ? "commander" : QString::number(robot);
    if (!commander) value.robotId = robot;
    return value;
}
bool send(ly27::StateStore &store, const google::protobuf::Message &message, qint64 &now, bool retained = false)
{
    ++now;
    return store.apply(ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
        QByteArray::fromStdString(message.SerializeAsString())), 1700000000000, now, retained);
}
ly27::wire::MapClickInfo marker(int receiver = 3, int target = 0)
{
    ly27::wire::MapClickInfo m;
    m.set_is_send_all(1); m.set_robot_id(std::string(1, char(receiver)) + std::string(6, '\0'));
    m.set_mode(1); m.set_type(target ? 2 : 1); m.set_enemy_id(target);
    m.set_map_x(target ? 0 : 12); m.set_map_y(target ? 0 : 6); return m;
}
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
}

class TacticalMapTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QProcess broker;
    quint16 port = 0;
    ly27::NetworkConfig network(QString name) const
    {
        ly27::NetworkConfig value;
        value.mqttHost = "127.0.0.1"; value.mqttPort = port; value.mqttClientId = name; return value;
    }
    ly27::TerminalConfig config(bool commander, int robot = 3) const
    {
        ly27::TerminalConfig value;
        value.identity = identity(commander, robot);
        value.network = network(commander ? "tactical-commander" : "tactical-" + QString::number(robot));
        value.logDirectory = directory.filePath(value.network.mqttClientId);
        value.logLevel = ly27::LogLevel::Warning;
        return value;
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
        QTRY_VERIFY_WITH_TIMEOUT(([&] { QTcpSocket socket; socket.connectToHost(QHostAddress::LocalHost, port); return socket.waitForConnected(30); })(), 3000);
    }
    void cleanupTestCase()
    {
        broker.terminate();
        if (!broker.waitForFinished(3000)) { broker.kill(); broker.waitForFinished(); }
    }
    void coordinateRoundTripBothViews()
    {
        for (bool blue : {false, true}) for (auto point : {QPointF(0, 0), QPointF(28, 15), QPointF(16.25, 10.5)}) {
            const auto projected = ly27::RobotMapModel::projectPosition(point.x(), point.y(), blue);
            const auto recovered = ly27::TacticalMapController::unprojectPosition(projected, blue);
            QVERIFY(QLineF(point, recovered).length() < .00001);
        }
    }
    void recipientsFollowingExpiryAndSessionIsolation()
    {
        for (int self : {3, 103}) {
            qint64 now = 1000000000;
            ly27::StateStore store(identity(false, self), nullptr, [&] { return now; });
            store.reset("marks"); store.setConnected(true);
            ly27::TacticalMapController controller(store, identity(false, self), nullptr, [&] { return now; });
            auto m = marker(self);
            QVERIFY(send(store, m, now)); QCOMPARE(controller.markers().size(), 1);
            const auto initial = controller.markers();
            m = marker(self == 3 ? 4 : 104); QVERIFY(send(store, m, now)); QCOMPARE(controller.markers(), initial);
            m = marker(self); m.set_robot_id(std::string(7, '\0')); m.set_is_send_all(0);
            QVERIFY(send(store, m, now)); QCOMPARE(controller.markers(), initial);
            m = marker(self); m.set_mode(4); QVERIFY(send(store, m, now)); QCOMPARE(controller.markers(), initial);
            m = marker(self); m.set_map_x(std::numeric_limits<float>::quiet_NaN());
            QVERIFY(send(store, m, now)); QCOMPARE(controller.markers(), initial);
            m = marker(self); QVERIFY(send(store, m, now, true)); QCOMPARE(controller.markers(), initial);
            controller.clickRobot(self); QCOMPARE(controller.selectedReceiver(), 0); // 操作手不能发标记。

            const int enemy = self == 3 ? 103 : 3;
            ly27::wire::RobotPosition p; p.set_robot_id(enemy); p.set_x(5); p.set_y(8);
            QVERIFY(send(store, p, now)); m = marker(self, enemy); QVERIFY(send(store, m, now));
            auto pos = ly27::RobotMapModel::projectPosition(5, 8, self >= 100);
            QCOMPARE(controller.markers().first().toMap()["mapX"].toDouble(), pos.x());
            p.set_x(9); QVERIFY(send(store, p, now));
            pos = ly27::RobotMapModel::projectPosition(9, 8, self >= 100);
            QCOMPARE(controller.markers().first().toMap()["mapX"].toDouble(), pos.x());
            controller.setBlueView(self < 100);
            QCOMPARE(controller.markers().first().toMap()["mapX"].toDouble(), 1 - pos.x());
            now += 3100000000LL; store.refresh(); controller.refresh(); QVERIFY(controller.markers().isEmpty());
            QVERIFY(send(store, p, now)); QCOMPARE(controller.markers().size(), 1); // 位置恢复后继续剩余显示时间。
            now += 7000000000LL; controller.refresh(); QVERIFY(controller.markers().isEmpty());
            m = marker(self); QVERIFY(send(store, m, now)); QCOMPARE(controller.markers().size(), 1);
            store.setConnected(false); QVERIFY(controller.markers().isEmpty());
            store.setConnected(true); controller.refresh(); QVERIFY(controller.markers().isEmpty());
            QVERIFY(send(store, m, now, true)); QVERIFY(controller.markers().isEmpty());
            QVERIFY(send(store, m, now)); QCOMPARE(controller.markers().size(), 1);
            store.reset("new-session"); QVERIFY(controller.markers().isEmpty());
            store.setConnected(true); QVERIFY(send(store, m, now));
            ly27::wire::GameStatus g; g.set_current_round(1); QVERIFY(send(store, g, now));
            g.set_current_round(2); QVERIFY(send(store, g, now)); QVERIFY(controller.markers().isEmpty());
        }
    }
    void pendingSelectionClearsBeforeFirstNotification()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(true), nullptr, [&] { return now; });
        store.reset("pending"); store.setConnected(true);
        ly27::TacticalMapController controller(store, identity(true), nullptr, [&] { return now; });
        ly27::wire::GameStatus g; g.set_current_round(1); QVERIFY(send(store, g, now));
        ly27::wire::RobotPosition p; p.set_robot_id(3); p.set_x(3); p.set_y(3); QVERIFY(send(store, p, now));
        controller.clickRobot(3); controller.clickMap(.5, .5);
        QCOMPARE(controller.selectedReceiver(), 3); QVERIFY(!controller.pending().isEmpty());
        store.setConnected(false); QCOMPARE(controller.selectedReceiver(), 0); QVERIFY(controller.pending().isEmpty());
        store.setConnected(true); QVERIFY(send(store, p, now)); controller.clickRobot(3);
        QCOMPARE(controller.selectedReceiver(), 3);
        g.set_current_round(2); QVERIFY(send(store, g, now)); QCOMPARE(controller.selectedReceiver(), 0);
    }
    void realMouseClicksAndMqttSynchronization()
    {
        ly27::TerminalRuntime commander(config(true)), operation(config(false)), other(config(false, 4));
        QVERIFY(commander.start(true, false)); QVERIFY(operation.start(true, false)); QVERIFY(other.start(true, false));
        ly27::TerminalViewModel cm(commander.stateStore(), commander.config().identity, "红方", "蓝方");
        ly27::TerminalViewModel om(operation.stateStore(), operation.config().identity, "红方", "蓝方");
        ly27::TerminalViewModel otherModel(other.stateStore(), other.config().identity, "红方", "蓝方");
        cm.tactics()->setCommandDispatcher(&commander.commands());
        ly27::MqttTransport relay; QList<ly27::wire::MapClickCmd> sent;
        // 独立 Broker 上回送同线格式；真实 Python 模拟器另有处理与集成测试。
        connect(&relay, &ly27::MqttTransport::messageReceived, this,
            [&](const QString &topic, const QByteArray &payload, qint64, qint64, int, bool) {
                if (topic != "MapClickCmd") return;
                ly27::wire::MapClickCmd command; QVERIFY(command.ParseFromString(payload.toStdString())); sent.append(command);
                QVERIFY(relay.publish("MapClickInfo", payload, 1) >= 0);
            });
        QVERIFY(relay.start(network("tactical-relay"), {"MapClickCmd"}));
        QTRY_VERIFY_WITH_TIMEOUT(commander.commands().isReady() && operation.commands().isReady() &&
            other.commands().isReady() && relay.snapshot()["state"] == "ready", 5000);
        const auto publish = [&](const google::protobuf::Message &message) {
            return relay.publish(QString::fromStdString(message.GetDescriptor()->name()),
                                 QByteArray::fromStdString(message.SerializeAsString()), 1) >= 0;
        };
        ly27::wire::GameStatus g; g.set_current_stage(4); g.set_current_round(1); g.set_stage_countdown_sec(300);
        g.set_is_paused(false);
        ly27::wire::GlobalUnitStatus u; u.set_base_health(8000); u.set_enemy_base_health(6000);
        u.set_outpost_r_health(800); u.set_outpost_b_health(600);
        for (int i = 0; i < 8; ++i) u.add_robot_health(200);
        QMap<int, QPointF> positions{{3, {5, 5}}, {4, {8, 4}}, {103, {22, 10}}};
        // 像真实模拟服务器一样持续推送，不能让操作期间的位置自然过期。
        const auto feed = [&] {
            QVERIFY(publish(g)); QVERIFY(publish(u));
            for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
                ly27::wire::RobotPosition p; p.set_robot_id(it.key()); p.set_x(it->x()); p.set_y(it->y()); QVERIFY(publish(p));
                ly27::wire::RobotStaticStatus s; s.set_robot_id(it.key()); s.set_max_health(400); s.set_level(2); QVERIFY(publish(s));
            }
        };
        QTimer liveFeed; connect(&liveFeed, &QTimer::timeout, this, feed); liveFeed.start(200); feed();
        QTRY_VERIFY(cm.robots()->data(cm.robots()->index(1, 0), ly27::RobotMapModel::PositionFresh).toBool());
        for (int i = 0; i < 8; ++i) QVERIFY(!cm.robots()->data(cm.robots()->index(i, 0), ly27::RobotMapModel::Self).toBool());
        QVERIFY(om.robots()->data(om.robots()->index(1, 0), ly27::RobotMapModel::Self).toBool());
        ly27::MapWindow cw(&cm), ow(&om); cw.show(); ow.show();
        QCOMPARE(cw.status(), QQuickView::Ready); QCOMPARE(ow.status(), QQuickView::Ready); QTest::qWait(120);
        auto *root = cw.rootObject(); auto *layer = item(root, "mapMarkerLayer"); QVERIFY(layer);
        const auto click = [&](const QString &name) {
            auto *target = item(root, name); if (!target) return false;
            QTest::mouseClick(&cw, Qt::LeftButton, Qt::NoModifier,
                target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint());
            return true;
        };
        QVERIFY(click("mapRobot3")); QCOMPARE(cm.tactics()->selectedReceiver(), 3);
        QVERIFY(item(root, "mapRobot3")->property("selected").toBool());
        const auto point = ly27::RobotMapModel::projectPosition(16, 4);
        QTest::mouseClick(&cw, Qt::LeftButton, Qt::NoModifier,
            layer->mapToScene({point.x() * layer->width(), point.y() * layer->height()}).toPoint());
        QCOMPARE(cm.tactics()->pending()["options"].toList().size(), 3);
        QTRY_VERIFY(item(root, "mapMarkerOption1")->x() < item(root, "mapMarkerOption3")->x());
        cw.resize(640, 480); QTest::qWait(100);
        auto *smallMenu = item(root, "mapMarkerMenu"); QVERIFY(smallMenu);
        QVERIFY(smallMenu->x() >= 0 && smallMenu->x() + smallMenu->width() <= layer->width());
        QVERIFY(smallMenu->y() >= 0 && smallMenu->y() + smallMenu->height() <= layer->height());
        cw.resize(1280, 800); QTest::qWait(100);
        QVERIFY(click("mapMarkerOption1"));
        QTRY_COMPARE_WITH_TIMEOUT(sent.size(), 1, 3000);
        QCOMPARE(sent.last().is_send_all(), 1u); QCOMPARE(sent.last().type(), 1u); QCOMPARE(sent.last().mode(), 1u);
        QCOMPARE(QByteArray::fromStdString(sent.last().robot_id()).toHex(), QByteArray("03000000000000"));
        QVERIFY(qAbs(sent.last().map_x() - 16) < .05); QVERIFY(qAbs(sent.last().map_y() - 4) < .05);
        QTRY_COMPARE(om.tactics()->markers().size(), 1); QVERIFY(otherModel.tactics()->markers().isEmpty());
        QCOMPARE(cm.tactics()->selectedReceiver(), 0);
        QDir().mkpath("test-artifacts"); QTest::qWait(120);
        QVERIFY(ow.grabWindow().save("test-artifacts/tactical-map-attack.png"));

        // 指挥手复选后，己方建筑直接防御，对方建筑直接攻击。
        for (int id : {10, 11, 110, 111}) {
            QTest::qWait(520); cm.tactics()->clickRobot(3); cm.tactics()->clickFacility(id);
            QTRY_COMPARE_WITH_TIMEOUT(sent.last().enemy_id(), quint32(id), 3000);
            QCOMPARE(sent.last().mode(), id < 100 ? 2u : 1u); QCOMPARE(sent.last().type(), 2u);
        }
        QTest::qWait(520);
        // 目标车辆位置必须持续新鲜；这里不借用之前已经过期的位置。
        for (int id : {3, 4, 103}) {
            ly27::wire::RobotPosition p; p.set_robot_id(id); p.set_x(id == 103 ? 22 : id == 4 ? 8 : 5); p.set_y(id == 103 ? 10 : 5);
            QVERIFY(publish(p));
        }
        QTRY_VERIFY(cm.robots()->data(cm.robots()->index(1, 0), ly27::RobotMapModel::PositionFresh).toBool());
        QTest::qWait(110);
        QVERIFY(click("mapRobot3")); QVERIFY(click("mapRobot4"));
        QTRY_COMPARE(sent.last().enemy_id(), 4u); QCOMPARE(sent.last().mode(), 2u);
        QTRY_COMPARE(om.tactics()->markers().first().toMap()["targetId"].toInt(), 4);
        QTest::qWait(110); QVERIFY(ow.grabWindow().save("test-artifacts/tactical-map-defense.png"));
        QTest::qWait(520);
        QVERIFY(click("mapRobot3")); QVERIFY(click("mapRobot103"));
        QCOMPARE(cm.tactics()->pending()["options"].toList(), QVariantList({1, 3}));
        QVERIFY(click("mapMarkerOption3")); QTRY_COMPARE(sent.last().enemy_id(), 103u); QCOMPARE(sent.last().mode(), 3u);
        QTRY_COMPARE(om.tactics()->markers().first().toMap()["targetId"].toInt(), 103);
        positions[103] = {20, 9};
        ly27::wire::RobotPosition moved; moved.set_robot_id(103); moved.set_x(20); moved.set_y(9); QVERIFY(publish(moved));
        const auto movedPoint = ly27::RobotMapModel::projectPosition(20, 9);
        QTRY_COMPARE(om.tactics()->markers().first().toMap()["mapX"].toDouble(), movedPoint.x());
        QTest::qWait(120); QVERIFY(ow.grabWindow().save("test-artifacts/tactical-map-warning.png"));
        QVERIFY(cw.grabWindow().save("test-artifacts/tactical-map-commander.png"));
        cm.tactics()->clickRobot(3); cm.tactics()->clickMap(.03, .03);
        auto *menu = item(root, "mapMarkerMenu"); QVERIFY(menu && menu->isVisible());
        QVERIFY(menu->x() >= 0 && menu->y() >= 0);
        QTest::keyClick(&cw, Qt::Key_Escape); QTRY_COMPARE(cm.tactics()->selectedReceiver(), 0);
        QVERIFY(cm.tactics()->pending().isEmpty());
        operation.stop(); QTRY_VERIFY(om.tactics()->markers().isEmpty());
        cw.setSource(QUrl()); ow.setSource(QUrl());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    TacticalMapTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_tactical_map.moc"
