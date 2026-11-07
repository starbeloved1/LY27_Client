#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickItem>
#include <QSet>
#include <QtTest>
#include <memory>

namespace {
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
struct Scene {
    qint64 now = 1000000000;
    ly27::TerminalIdentity identity;
    ly27::StateStore store;
    ly27::TerminalViewModel model;
    ly27::MapWindow window;
    explicit Scene(ly27::Team team = ly27::Team::Red, ly27::Role role = ly27::Role::Infantry, int number = 4)
        : identity{role, team, "player-ui", role == ly27::Role::Commander ? std::optional<int>{}
            : std::optional<int>{number + (team == ly27::Team::Blue ? 100 : 0)}},
          store(identity, nullptr, [this] { return now; }), model(store, identity, "红方测试队", "蓝方测试队"), window(&model)
    { store.reset("player-ui"); store.setConnected(true); window.show(); }
    ~Scene() { window.setSource(QUrl()); }
    bool send(const google::protobuf::Message &p, bool retained = false)
    {
        return store.apply(ly27::MessageCodec::decode(QString::fromStdString(p.GetDescriptor()->name()),
            QByteArray::fromStdString(p.SerializeAsString())), 1700000000000, ++now, retained);
    }
    QQuickItem *get(const QString &name) { return item(window.rootObject(), name); }
};
}

class PlayerUiTest : public QObject {
    Q_OBJECT
private slots:
    void playerModuleLampsUse27States()
    {
        const QStringList keys{"armor", "videoTransmission", "smallShooter", "bigShooter", "rfid", "uwb",
                               "lightStrip", "powerManager", "capacitor", "mainController"};
        const QList<int> states{2, 1, 1, 0, 0, 1, 2, 1, 0, 2};
        for (auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            Scene s(team); QTest::keyClick(&s.window, Qt::Key_P);
            auto *page = s.get("videoPage"); QVERIFY(page);
            auto *status = item(page, "playerRobotStatus"); QVERIFY(status);
            auto *grid = item(status, "playerModuleGrid"); QVERIFY(grid);
            QCOMPARE(grid->property("columns").toInt(), 5);
            const auto allUnknown = [&] {
                for (const auto &key : keys) {
                    const auto *lamp = item(grid, "playerModule_" + key);
                    if (!lamp || lamp->property("moduleState").toInt() != -1 ||
                        lamp->property("stateText").toString() != "未知") return false;
                }
                return true;
            };
            QVERIFY(allUnknown());
            ly27::wire::RobotStaticStatus stat; stat.set_robot_id(s.identity.robotId.value());
            stat.set_level(3); stat.set_max_health(400); stat.set_max_buffer_energy(60); stat.set_max_power(80);
            QVERIFY(s.send(stat));
            ly27::wire::RobotDynamicStatus current; current.set_current_health(180);
            current.set_current_buffer_energy(40); current.set_is_out_of_combat(true);
            QVERIFY(s.send(current));
            ly27::wire::RobotModuleStatus p;
            p.set_armor(2); p.set_video_transmission(1); p.set_small_shooter(1); p.set_big_shooter(0);
            p.set_rfid(0); p.set_uwb(1); p.set_light_strip(2); p.set_power_manager(1);
            p.set_capacitor(0); p.set_main_controller(2); p.set_modular_42mm(1);
            // 不依赖伤害消息或视频帧，直接消费当前本车的模块消息。
            QVERIFY(s.send(p));
            for (int i = 0; i < keys.size(); ++i) {
                auto *cell = item(grid, "playerModule_" + keys[i]); QVERIFY(cell);
                QCOMPARE(cell->property("moduleState").toInt(), states[i]);
                QCOMPARE(cell->property("stateText").toString(), states[i] == 1 ? QString("在线")
                    : states[i] == 0 ? QString("离线") : QString("安装异常"));
                const auto color = states[i] == 1 ? "green" : states[i] == 0 ? "red" : "yellow";
                auto *lamp = item(cell, "playerModuleLamp_" + keys[i]); QVERIFY(lamp);
                QCOMPARE(lamp->property("source").toUrl().fileName(),
                    QString("statusbar_signallamp_%1_single.png").arg(color));
                QVERIFY(lamp->property("status").toInt() != 3);
            }
            QVERIFY(!item(grid, "playerModule_modular42mmPermission"));
            ly27::wire::RobotDynamicStatus shooting;
            shooting.set_last_projectile_fire_rate(24.2); QVERIFY(s.send(shooting));
            QCOMPARE(item(page, "playerMuzzleVelocity")->property("text").toString(), QString("24.2"));
            QCOMPARE(item(grid, "playerModule_bigShooter")->property("moduleState").toInt(), 0);
            QCOMPARE(item(grid, "playerModuleLabel_bigShooter")->property("text").toString(), QString("42mm"));
            QDir().mkpath("test-artifacts");
            for (auto size : {QSize(1920, 1080), QSize(1280, 800), QSize(640, 480)}) {
                s.window.resize(size); QTest::qWait(60);
                QVERIFY(QRectF(QPointF(), status->size()).contains(grid->mapRectToItem(status, QRectF(QPointF(), grid->size()))));
                for (const auto &key : keys) {
                    auto *cell = item(grid, "playerModule_" + key);
                    QVERIFY(QRectF(QPointF(), QSizeF(size)).contains(cell->mapRectToScene(QRectF(QPointF(), cell->size()))));
                    auto *label = item(cell, "playerModuleLabel_" + key);
                    QVERIFY(label->property("contentWidth").toDouble() <= label->width());
                }
                QVERIFY(s.window.grabWindow().save(QString("test-artifacts/player-modules-%1-%2.png")
                    .arg(team == ly27::Team::Red ? "red" : "blue").arg(size.width())));
            }
            s.now += 3100000000LL;
            ly27::wire::RobotDynamicStatus dynamic; dynamic.set_current_health(200);
            QVERIFY(s.send(dynamic)); QVERIFY(allUnknown()); // 更新血量不延长模块有效期。
            QVERIFY(s.send(p, true)); QVERIFY(allUnknown());
            auto partial = p; partial.clear_armor(); partial.set_rfid(99); QVERIFY(s.send(partial));
            QCOMPARE(item(grid, "playerModule_armor")->property("moduleState").toInt(), -1);
            QCOMPARE(item(grid, "playerModule_rfid")->property("moduleState").toInt(), -1);
            s.store.setConnected(false); QVERIFY(allUnknown());
            s.store.setConnected(true); QVERIFY(allUnknown());
            QVERIFY(s.send(p));
            ly27::wire::GameStatus game; game.set_current_round(1); QVERIFY(s.send(game));
            game.set_current_round(2); QVERIFY(s.send(game)); QVERIFY(allUnknown());
        }
    }
    void outpostRebuildAndTemporaryRecapture()
    {
        for (auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            Scene s(team);
            auto status = [team](bool blue, bool temporary) { return blue == (team == ly27::Team::Blue)
                ? (temporary ? 2 : 1) : (temporary ? 3 : 4); };
            ly27::wire::GlobalUnitStatus u;
            u.set_outpost_r_health(1000); u.set_outpost_r_status(status(false, false));
            u.set_outpost_b_health(1000); u.set_outpost_b_status(status(true, false));
            auto check = [&](const QString &owner, int health) {
                auto *station = s.get("mapBOutpost");
                return s.model.match()["bOutpostOwner"].toString() == owner &&
                    station->property("health").toInt() == health &&
                    station->property("blue").toBool() == (owner == "blue") &&
                    item(station, "facilityMapIcon")->property("source").toUrl().fileName() == owner + "_map_outpost.png";
            };
            QVERIFY(s.send(u)); QVERIFY(check("blue", 1000));
            u.set_outpost_b_status(status(false, true)); QVERIFY(s.send(u)); QVERIFY(check("blue", 1000));
            u.set_outpost_b_status(status(true, false)); QVERIFY(s.send(u)); QVERIFY(check("blue", 1000));
            // 蓝方前哨被击毁时仍保留最后的建筑拥有方，直到新完全控制方的存活快照确认重建。
            u.set_outpost_b_health(0); u.set_outpost_b_status(status(true, true)); QVERIFY(s.send(u)); QVERIFY(check("blue", 0));
            u.set_outpost_b_health(500); u.set_outpost_b_status(status(false, false)); QVERIFY(s.send(u)); QVERIFY(check("red", 500));
            auto *station = s.get("mapBOutpost");
            QCOMPARE(station->property("accent").value<QColor>(), QColor("#DE3A47"));
            QCOMPARE(station->property("ratio").toDouble(), .5); // 不伪造最大血量或服务器当前血量。
            u.set_outpost_b_health(450); u.set_outpost_b_status(status(true, true)); QVERIFY(s.send(u)); QVERIFY(check("red", 450));
            u.set_outpost_b_status(status(false, false)); QVERIFY(s.send(u)); QVERIFY(check("red", 450));
            // 1 Hz 可能跳过死亡瞬间，完全控制方更替且已存活的同步结果仍应恢复正确归属。
            u.set_outpost_b_health(500); u.set_outpost_b_status(status(true, false)); QVERIFY(s.send(u)); QVERIFY(check("blue", 500));
            ly27::TerminalViewModel restored(s.store, s.identity, "红", "蓝");
            QCOMPARE(restored.match()["bOutpostOwner"].toString(), QString("blue"));
            u.set_outpost_b_status(status(false, true)); QVERIFY(s.send(u)); QVERIFY(check("blue", 500));
            ly27::TerminalViewModel ambiguous(s.store, s.identity, "红", "蓝");
            QCOMPARE(ambiguous.match()["bOutpostOwner"].toString(), QString("unknown"));
            s.model.setBlueView(!s.model.blueView()); QVERIFY(check("blue", 500));
            s.now += 3100000000LL; s.model.refresh();
            QCOMPARE(s.model.match()["bOutpostOwner"].toString(), QString("unknown"));
            QVERIFY(s.send(u)); QVERIFY(check("blue", 500));
            s.store.setConnected(false); QCOMPARE(s.model.match()["bOutpostOwner"].toString(), QString("unknown"));
            s.store.setConnected(true); QVERIFY(s.send(u, true));
            QCOMPARE(s.model.match()["bOutpostOwner"].toString(), QString("unknown"));
            QVERIFY(s.send(u)); // 新连接不能借上一连接的完全控制历史判断临时占领下的建筑归属。
            QCOMPARE(s.model.match()["bOutpostOwner"].toString(), QString("unknown"));
            u.set_outpost_b_status(status(false, false)); QVERIFY(s.send(u)); QVERIFY(check("red", 500));
            ly27::wire::GameStatus game; game.set_current_round(1); QVERIFY(s.send(game));
            game.set_current_round(2); QVERIFY(s.send(game));
            QCOMPARE(s.model.match()["bOutpostOwner"].toString(), QString("unknown"));
            s.store.reset("another-session");
            QCOMPARE(s.model.match()["bOutpostOwner"].toString(), QString("unknown"));
        }
    }
    void noticeBodyCentersOnWholeFrame()
    {
        Scene s;
        for (const QString &name : {QString("BattleNoticeCard"), QString("EventResultPanel")}) {
            QQmlComponent component(s.window.engine(), QUrl("qrc:/ly27/ui/" + name + ".qml"));
            QVERIFY2(!component.isError(), qPrintable(component.errorString()));
            std::unique_ptr<QObject> object(component.create()); QVERIFY(object);
            auto *card = qobject_cast<QQuickItem *>(object.get()); QVERIFY(card);
            card->setParentItem(s.window.rootObject());
            card->setWidth(340);
            for (bool compact : {false, true}) for (const QString &detail : {QString{}, QString("倒计时4秒")}) {
                card->setProperty("compact", compact); card->setHeight(compact ? 50 : 84);
                if (name == "BattleNoticeCard") {
                    card->setProperty("heading", "红方前哨阵地");
                    card->setProperty("message", "我方正在占领红方前哨阵地！"); card->setProperty("detail", detail);
                } else card->setProperty("result", QVariantMap{{"heading", "能量机关激活"}, {"message", "小能量机关已激活"}, {"detail", detail}});
                QTest::qWait(5);
                auto *body = item(card, "noticeBody"); QVERIFY(body);
                const auto bounds = body->mapRectToItem(card, QRectF(QPointF(), body->size()));
                QVERIFY(qAbs(bounds.center().y() - card->height() / 2) < .01);
                QVERIFY(qAbs(bounds.center().x() - card->width() / 2) < .01);
            }
        }
    }
    void playerPageAdapts27UnitsAndReadOnlyStatus()
    {
        for (auto role : {ly27::Role::Heavy, ly27::Role::Infantry, ly27::Role::Commander}) {
            Scene s(ly27::Team::Blue, role, role == ly27::Role::Heavy ? 1 : 4);
            QCOMPARE(s.model.playerHud()["self"].toMap()["currentHP"].toInt(), -1);
            ly27::wire::GameStatus game; game.set_current_round(1); game.set_total_rounds(3);
            game.set_current_stage(4); game.set_is_paused(false); game.set_stage_countdown_sec(287); game.set_stage_elapsed_sec(133);
            QVERIFY(s.send(game));
            ly27::wire::GlobalUnitStatus u; u.set_base_health(8000); u.set_enemy_base_health(9000);
            u.set_outpost_r_health(1000); u.set_outpost_b_health(500); u.set_outpost_r_status(4); u.set_outpost_b_status(1);
            for (int n = 0; n < 8; ++n) u.add_robot_health(200 + n * 10);
            QVERIFY(s.send(u));
            ly27::wire::RobotStaticStatus stat; stat.set_robot_id(s.identity.robotId.value_or(107));
            stat.set_level(3); stat.set_max_health(400); stat.set_max_heat(200); stat.set_max_buffer_energy(60); stat.set_max_power(80);
            QVERIFY(s.send(stat));
            ly27::wire::RobotDynamicStatus dynamic; dynamic.set_current_health(180); dynamic.set_current_heat(50);
            dynamic.set_current_buffer_energy(20); dynamic.set_current_experience(100); dynamic.set_experience_for_upgrade(20);
            dynamic.set_last_projectile_fire_rate(24.2); dynamic.set_remaining_ammo(16); dynamic.set_is_out_of_combat(false);
            QVERIFY(s.send(dynamic));
            const auto self = s.model.playerHud()["self"].toMap();
            if (role != ly27::Role::Commander) {
                QCOMPARE(self["currentHP"].toInt(), 180); QCOMPARE(self["maxHP"].toInt(), 400);
                QCOMPARE(self["maxExp"].toDouble(), 120); QCOMPARE(self["remainingAmmo"].toInt(), 16);
            } else { QVERIFY(!self["bound"].toBool()); QCOMPARE(self["currentHeat"].toInt(), -1); }
            QTest::keyClick(&s.window, Qt::Key_P); QVERIFY(s.window.videoVisible());
            auto *page = s.get("videoPage"); QVERIFY(page);
            QVERIFY(!s.get("matchHeader")->isVisible()); QVERIFY(!s.get("battleAnalysisPanel")->isVisible());
            for (int id : {1,3,4,7,101,103,104,107}) QVERIFY(item(page, "playerUnit" + QString::number(id)));
            for (int id : {2,5,6,102,105,106}) QVERIFY(!item(page, "playerUnit" + QString::number(id)));
            auto *status = item(page, "playerRobotStatus"); QVERIFY(status);
            QCOMPARE(status->isVisible(), role != ly27::Role::Commander);
            if (role != ly27::Role::Commander) {
                QCOMPARE(item(page, "playerAllowedAmmo")->property("text").toString(), QString("16"));
                QCOMPARE(item(page, "playerHealthText")->property("text").toString(), QString("180 / 400"));
                QVERIFY(qAbs(status->property("expRatio").toDouble() - 100.0/120) < .0001);
            }
            QDir().mkpath("test-artifacts");
            for (auto size : {QSize(1920,1080), QSize(1280,800), QSize(640,480)}) {
                s.window.resize(size); QTest::qWait(80);
                auto *canvas = item(page, "playerHudCanvas"); QVERIFY(canvas);
                QVERIFY(QRectF(0,0,size.width(),size.height()).contains(canvas->mapRectToScene(QRectF(QPointF(),canvas->size()))));
                QVERIFY(s.window.grabWindow().save(QString("test-artifacts/player-%1-%2.png").arg(int(role)).arg(size.width())));
            }
            for (auto *image : page->findChildren<QQuickItem *>()) {
                if (image->inherits("QQuickImage") && image->property("source").toUrl().isValid())
                    QVERIFY2(image->property("status").toInt() != 3, qPrintable(image->property("source").toUrl().toString()));
            }
            const auto before = s.model.playerHud();
            QTest::keyClick(&s.window, Qt::Key_H); QCOMPARE(s.model.playerHud(), before);
            QTest::keyClick(&s.window, Qt::Key_P); QVERIFY(!s.window.videoVisible());
            // 共享地图重组后，背景不能排在图片后面把图片和标记全部盖住。
            const auto restoredMap = s.window.grabWindow();
            auto *area = s.get("mapArea"); QVERIFY(area);
            QSet<QRgb> mapColors;
            for (int x = 1; x < 9; ++x) for (int y = 1; y < 9; ++y) {
                const auto point = area->mapToScene(QPointF(area->width() * x / 10, area->height() * y / 10));
                mapColors.insert(restoredMap.pixel(qRound(point.x()), qRound(point.y())));
            }
            QVERIFY(mapColors.size() > 16);
            game.set_current_stage(5); game.set_game_result(1); QVERIFY(s.send(game));
            auto *phase = s.get("matchPhaseOverlay"); QVERIFY(phase && phase->isVisible());
            phase->setProperty("dismissedToken", phase->property("token")); QVERIFY(!phase->isVisible());
            QTest::keyClick(&s.window, Qt::Key_P); QVERIFY(!phase->isVisible());
            QTest::keyClick(&s.window, Qt::Key_P); QVERIFY(!phase->isVisible());
        }
    }
};

int main(int argc, char **argv) { QGuiApplication app(argc,argv); PlayerUiTest test; return QTest::qExec(&test,argc,argv); }
#include "test_player_ui.moc"
