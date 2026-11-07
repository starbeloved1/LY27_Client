#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlComponent>
#include <QQuickItem>
#include <QtTest>
#include <memory>

namespace {
ly27::TerminalIdentity identity(ly27::Team team, bool commander)
{
    ly27::TerminalIdentity i; i.team = team;
    i.role = commander ? ly27::Role::Commander : ly27::Role::Infantry;
    if (!commander) i.robotId = team == ly27::Team::Red ? 3 : 103;
    i.stationId = "injury-test"; return i;
}
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
struct Scene {
    qint64 now = 1000000000;
    ly27::StateStore store;
    ly27::TerminalViewModel model;
    ly27::MapWindow window;
    explicit Scene(ly27::Team team = ly27::Team::Red, bool commander = false)
        : store(identity(team, commander), nullptr, [this] { return now; }),
          model(store, identity(team, commander), "红", "蓝"), window(&model)
    { store.reset("injury"); store.setConnected(true); window.show(); }
    ~Scene() { window.setSource(QUrl()); }
    bool send(const google::protobuf::Message &message, bool retained = false)
    {
        ++now;
        return store.apply(ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
            QByteArray::fromStdString(message.SerializeAsString())), 1700000000000, now, retained);
    }
    QQuickItem *get(const QString &name) { return item(window.rootObject(), name); }
    QString text(const QString &name) { return get(name)->property("text").toString(); }
};
ly27::wire::RobotInjuryStat injury()
{
    ly27::wire::RobotInjuryStat p;
    p.set_total_damage(1000); p.set_small_projectile_damage(320); p.set_large_projectile_damage(80);
    p.set_collision_damage(100); p.set_dart_splash_damage(40); p.set_module_offline_damage(20);
    p.set_offline_damage(30); p.set_penalty_damage(10); p.set_server_kill_damage(400); p.set_killer_id(107);
    return p;
}
ly27::wire::RobotModuleStatus modules()
{
    ly27::wire::RobotModuleStatus p;
    p.set_power_manager(1); p.set_rfid(0); p.set_light_strip(2);
    p.set_small_shooter(1); p.set_big_shooter(0); p.set_uwb(1); p.set_armor(2);
    p.set_video_transmission(1); p.set_capacitor(0); p.set_main_controller(2);
    p.set_modular_42mm(1);
    return p;
}
}

class InjuryTest : public QObject {
    Q_OBJECT
private slots:
    void holdReleaseRepeatAndFocusLoss()
    {
        Scene s; auto *panel = s.get("injuryPanel"); QVERIFY(panel);
        QVERIFY(!panel->isVisible());
        for (int key : {Qt::Key_QuoteLeft, Qt::Key_AsciiTilde, Qt::Key_Dead_Grave}) {
            // QtTest 的 ASCII 转换不支持死重音键，直接投递对应的原生键盘事件。
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, "`");
            QCoreApplication::sendEvent(&s.window, &press); QVERIFY(panel->isVisible());
            QKeyEvent repeat(QEvent::KeyRelease, key, Qt::NoModifier, "`", true);
            QCoreApplication::sendEvent(&s.window, &repeat); QVERIFY(panel->isVisible());
            QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, "`");
            QCoreApplication::sendEvent(&s.window, &release); QVERIFY(!panel->isVisible());
        }
        QTest::keyClick(&s.window, Qt::Key_QuoteLeft, Qt::ControlModifier);
        QVERIFY(!panel->isVisible());
        QTest::keyPress(&s.window, Qt::Key_AsciiTilde, Qt::ShiftModifier); QVERIFY(panel->isVisible());
        QEvent deactivated(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(&s.window, &deactivated); QVERIFY(!panel->isVisible());
        QTest::keyRelease(&s.window, Qt::Key_AsciiTilde, Qt::ShiftModifier);
        // 图传默认占满窗口，按住快捷键时仍允许临时查看右侧面板。
        QTest::keyClick(&s.window, Qt::Key_P); QVERIFY(s.window.videoVisible());
        QTest::keyPress(&s.window, Qt::Key_QuoteLeft); QVERIFY(panel->isVisible());
        QTest::keyRelease(&s.window, Qt::Key_QuoteLeft); QVERIFY(!panel->isVisible());
        QTest::keyClick(&s.window, Qt::Key_P);
        QTest::keyPress(&s.window, Qt::Key_QuoteLeft); QVERIFY(panel->isVisible());
        s.window.hide(); QVERIFY(!panel->isVisible());
    }
    void editingTextDoesNotOpenPanel()
    {
        Scene s;
        QQmlComponent component(s.window.engine());
        component.setData("import QtQuick; TextInput { width: 100; height: 30 }", QUrl());
        std::unique_ptr<QObject> owner(component.create()); QVERIFY(owner);
        auto *input = qobject_cast<QQuickItem *>(owner.get()); QVERIFY(input);
        input->setParentItem(s.window.rootObject());
        input->forceActiveFocus();
        QCOMPARE(s.window.activeFocusItem(), input);
        QTest::keyPress(&s.window, Qt::Key_QuoteLeft);
        QVERIFY(!s.get("injuryPanel")->isVisible());
        QTest::keyRelease(&s.window, Qt::Key_QuoteLeft);
    }
    void allSourcesAndPercentageUseCurrentRobot()
    {
        for (auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            Scene s(team); QVERIFY(s.send(injury())); QVERIFY(s.send(modules()));
            QCOMPARE(s.text("injuryTotal"), QString("1000"));
            QVERIFY(s.text("injuryRobot").startsWith(team == ly27::Team::Red ? "红方 3 号" : "蓝方 3 号"));
            QCOMPARE(s.text("injuryKiller"), QString("击杀者：蓝方 7 号"));
            const char *keys[] = {"smallProjectile", "largeProjectile", "collision", "dartSplash", "moduleOffline", "offline", "penalty", "serverKill"};
            const int amounts[] = {320, 80, 100, 40, 20, 30, 10, 400};
            for (int n = 0; n < 8; ++n) {
                QCOMPARE(s.text(QString("injuryValue_") + keys[n]), QString::number(amounts[n]));
                QCOMPARE(s.text(QString("injuryPercent_") + keys[n]), QString::number(amounts[n] / 10.0, 'f', 1) + "%");
            }
            s.model.setBlueView(!s.model.blueView());
            QCOMPARE(s.text("injuryTotal"), QString("1000"));
            QTest::keyPress(&s.window, Qt::Key_QuoteLeft);
            for (auto size : {QSize(1280, 800), QSize(1920, 1080), QSize(640, 480), QSize(640, 900)}) {
                s.window.resize(size); QTest::qWait(100);
                auto *panel = s.get("injuryPanel");
                QVERIFY(QRectF(QPointF(), QSizeF(size)).contains(panel->mapRectToScene(QRectF(QPointF(), panel->size()))));
                const auto bounds = panel->mapRectToScene(QRectF(QPointF(), panel->size()));
                QVERIFY(qAbs(bounds.right() - (size.width() - 12)) < .01);
                auto *frame = s.get("injuryPanelFrame"); QVERIFY(frame);
                QCOMPARE(frame->mapRectToScene(QRectF(QPointF(), frame->size())), bounds);
                const auto moduleRows = s.model.injury()["modules"].toList();
                QCOMPARE(moduleRows.size(), 10);
                for (const auto &entry : moduleRows) {
                    const QString key = entry.toMap()["key"].toString();
                    auto *chip = s.get("injuryModule_" + key); QVERIFY(chip);
                    QVERIFY(bounds.contains(chip->mapRectToScene(QRectF(QPointF(), chip->size()))));
                    auto *label = s.get("injuryModuleLabel_" + key);
                    auto *status = s.get("injuryModuleStatus_" + key);
                    QVERIFY(label && status);
                    const auto labelBounds = label->mapRectToScene(QRectF(QPointF(), label->size()));
                    const auto statusBounds = status->mapRectToScene(QRectF(QPointF(), status->size()));
                    QVERIFY(!labelBounds.intersects(statusBounds));
                    QVERIFY(label->property("contentWidth").toReal() <= label->width());
                    QVERIFY(status->property("contentWidth").toReal() <= status->width());
                }
                QDir().mkpath("test-artifacts");
                QVERIFY(s.window.grabWindow().save(QString("test-artifacts/injury-%1-%2.png").arg(int(team)).arg(size.width())));
            }
            QTest::keyRelease(&s.window, Qt::Key_QuoteLeft);
        }
    }
    void moduleStatesAndLifecycle()
    {
        const QStringList keys{"armor", "videoTransmission", "smallShooter", "bigShooter", "rfid", "uwb",
                               "lightStrip", "powerManager", "capacitor", "mainController"};
        const QList<int> expected{2, 1, 1, 0, 0, 1, 2, 1, 0, 2};
        for (auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            Scene s(team);
            auto assertUnknown = [&] {
                for (const auto &key : keys) {
                    if (s.text("injuryModuleStatus_" + key) != "未知" ||
                        s.get("injuryModule_" + key)->property("moduleState").toInt() != -1) return false;
                }
                return true;
            };
            QVERIFY(assertUnknown()); QVERIFY(s.send(injury())); QVERIFY(s.send(modules()));
            for (int n = 0; n < keys.size(); ++n) {
                QCOMPARE(s.get("injuryModule_" + keys[n])->property("moduleState").toInt(), expected[n]);
                QCOMPARE(s.text("injuryModuleStatus_" + keys[n]), expected[n] == 0 ? QString("离线")
                    : expected[n] == 1 ? QString("在线") : QString("安装异常"));
            }
            s.model.setBlueView(!s.model.blueView());
            QCOMPARE(s.text("injuryModuleStatus_armor"), QString("安装异常"));
            // 模块状态和伤害统计独立更新、独立过期，不用一个 Topic 刷新另一个的有效性。
            s.now += 2000000000LL; QVERIFY(s.send(modules()));
            s.now += 1100000000LL; s.model.refresh();
            QCOMPARE(s.text("injuryTotal"), QString("—"));
            QCOMPARE(s.text("injuryModuleStatus_videoTransmission"), QString("在线"));
            s.now += 2100000000LL; s.model.refresh(); QVERIFY(assertUnknown());
            QVERIFY(s.send(modules(), true)); QVERIFY(assertUnknown());
            auto partial = modules(); partial.clear_armor(); partial.set_rfid(99);
            QVERIFY(s.send(partial));
            QCOMPARE(s.text("injuryModuleStatus_armor"), QString("未知"));
            QCOMPARE(s.text("injuryModuleStatus_rfid"), QString("未知"));
            QVERIFY(!s.get("injuryModule_modular42mmPermission"));
            QVERIFY(!s.get("injuryModule_wifi")); QVERIFY(!s.get("injuryModule_armor0"));
            s.store.setConnected(false); QVERIFY(assertUnknown());
            s.store.setConnected(true); QVERIFY(assertUnknown());
            QVERIFY(s.send(modules()));
            ly27::wire::GameStatus game; game.set_current_round(1); QVERIFY(s.send(game));
            game.set_current_round(2); QVERIFY(s.send(game)); QVERIFY(assertUnknown());
            QVERIFY(s.send(modules())); s.store.reset("next-module-session"); QVERIFY(assertUnknown());
        }
        Scene commander(ly27::Team::Red, true); QVERIFY(commander.send(modules()));
        for (const auto &key : keys) QCOMPARE(commander.text("injuryModuleStatus_" + key), QString("未知"));
    }
    void missingStaleRetainedAndLifecycle()
    {
        Scene s;
        QCOMPARE(s.text("injuryTotal"), QString("—"));
        auto p = injury(); p.clear_total_damage(); p.clear_collision_damage();
        QVERIFY(s.send(p));
        QCOMPARE(s.text("injuryValue_smallProjectile"), QString("320"));
        QCOMPARE(s.text("injuryPercent_smallProjectile"), QString("—"));
        QCOMPARE(s.text("injuryValue_collision"), QString("—"));
        p.set_total_damage(0); p.set_small_projectile_damage(0); p.set_killer_id(0);
        QVERIFY(s.send(p));
        QCOMPARE(s.text("injuryPercent_smallProjectile"), QString("0.0%"));
        QCOMPARE(s.text("injuryPercent_largeProjectile"), QString("—"));
        QCOMPARE(s.text("injuryKiller"), QString("击杀者：未知／环境伤害"));
        p.set_total_damage(4294967295U); p.set_small_projectile_damage(4294967295U);
        QVERIFY(s.send(p)); QCOMPARE(s.text("injuryTotal"), QString("4294967295"));
        QCOMPARE(s.text("injuryPercent_smallProjectile"), QString("100.0%"));
        s.now += 3100000000LL; s.model.refresh();
        QCOMPARE(s.text("injuryTotal"), QString("—"));
        QVERIFY(s.send(p, true)); QCOMPARE(s.text("injuryTotal"), QString("—"));
        QVERIFY(s.send(p)); s.store.setConnected(false); QCOMPARE(s.text("injuryTotal"), QString("—"));
        s.store.setConnected(true); QCOMPARE(s.text("injuryTotal"), QString("—"));
        QVERIFY(s.send(p));
        ly27::wire::GameStatus g; g.set_current_round(1); QVERIFY(s.send(g));
        g.set_current_round(2); QVERIFY(s.send(g)); QCOMPARE(s.text("injuryTotal"), QString("—"));
        QVERIFY(s.send(p)); s.store.reset("next"); QCOMPARE(s.text("injuryTotal"), QString("—"));
        Scene commander(ly27::Team::Red, true); QVERIFY(commander.send(injury()));
        QCOMPARE(commander.text("injuryTotal"), QString("—"));
        QVERIFY(commander.text("injuryRobot").startsWith("当前身份未绑定机器人"));
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    InjuryTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_injury.moc"
