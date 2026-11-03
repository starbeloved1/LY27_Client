#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QGuiApplication>
#include <QDir>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QtTest>

namespace {
ly27::TerminalIdentity identity(ly27::Team team = ly27::Team::Red)
{
    ly27::TerminalIdentity result;
    result.team = team;
    result.role = ly27::Role::Infantry;
    result.stationId = "desktop-test";
    result.robotId = team == ly27::Team::Red ? 3 : 103;
    return result;
}
bool send(ly27::StateStore &store, const google::protobuf::Message &message, qint64 now,
          bool retained = false)
{
    const auto topic = QString::fromStdString(message.GetDescriptor()->name());
    return store.apply(ly27::MessageCodec::decode(topic, QByteArray::fromStdString(message.SerializeAsString())),
                       1700000000000, now, retained);
}
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems())
        if (auto *found = item(child, name)) return found;
    return nullptr;
}
}

class DesktopTest : public QObject {
    Q_OBJECT
private slots:
    void hudLayoutAndHealthBars()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("hud"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(), "凌云战队 · 红方测试队名", "蓝方测试战队");
        ly27::wire::GameStatus game;
        game.set_current_round(2); game.set_total_rounds(3); game.set_stage_countdown_sec(287);
        game.set_current_stage(4); game.set_is_paused(true); game.set_red_score(1); game.set_blue_score(0);
        QVERIFY(send(store, game, now));
        ly27::wire::GlobalUnitStatus units;
        units.set_base_health(8321); units.set_enemy_base_health(6789);
        units.set_base_status(0); units.set_enemy_base_status(0);
        units.set_outpost_r_health(765); units.set_outpost_b_health(0);
        units.set_total_cp_ally(1250); units.set_total_cp_enemy(980);
        const int health[] = {0, 150, 300, 450, 600, 500, 120, 350};
        const int ids[] = {1, 3, 4, 7, 101, 103, 104, 107};
        for (int hp : health) units.add_robot_health(hp);
        QVERIFY(send(store, units, now));
        ly27::wire::GlobalLogisticsStatus logistics;
        logistics.set_remaining_economy(550);
        QVERIFY(send(store, logistics, now));
        for (int row = 0; row < 8; ++row) {
            ly27::wire::RobotPosition position;
            position.set_robot_id(ids[row]); position.set_x(2 + row * 3); position.set_y(2 + row * 1.4);
            QVERIFY(send(store, position, now));
            ly27::wire::RobotStaticStatus stat;
            stat.set_robot_id(ids[row]); stat.set_level(row + 1); stat.set_max_health(600);
            QVERIFY(send(store, stat, now));
        }
        ly27::MapWindow window(&model);
        QCOMPARE(window.status(), QQuickView::Ready);
        window.show();
        auto *root = window.rootObject();
        auto rect = [](QQuickItem *i) { return i->mapRectToScene(QRectF(QPointF(), i->size())); };
        const QStringList names{"matchRound", "matchRedScore", "matchTimer", "matchBlueScore", "matchStage",
                                "redTeamName", "blueTeamName", "allyEconomyLabel", "allyEconomyValue",
                                "redControlPointsLabel", "redControlPointsValue", "blueControlPointsLabel", "blueControlPointsValue"};
        QVERIFY(QDir().mkpath("test-artifacts"));
        for (const QSize size : {QSize(1920, 1080), QSize(1280, 800), QSize(1050, 700), QSize(1024, 768), QSize(640, 900)}) {
            window.resize(size);
            QTest::qWait(220);
            const auto header = rect(item(root, "matchHeader"));
            for (int a = 0; a < names.size(); ++a) {
                auto *text = item(root, names[a]);
                QVERIFY(text);
                QVERIFY2(header.contains(rect(text)), qPrintable(names[a]));
                if (!names[a].contains("TeamName"))
                    QVERIFY2(text->property("contentWidth").toReal() <= text->width() + 1, qPrintable(names[a]));
                for (int b = a + 1; b < names.size(); ++b)
                    QVERIFY2(!rect(text).intersects(rect(item(root, names[b]))), qPrintable(names[a] + " / " + names[b]));
            }
            QCOMPARE(item(root, "matchTimer")->property("text").toString(), QString("04:47"));
            QCOMPARE(item(root, "matchStage")->property("text").toString(), QString("比赛暂停"));
            QCOMPARE(item(root, "allyEconomyValue")->property("text").toString(), QString("550"));
            QCOMPARE(item(root, "redControlPointsValue")->property("text").toString(), QString("1250"));
            QCOMPARE(item(root, "blueControlPointsValue")->property("text").toString(), QString("980"));
            for (const auto &name : {"redBaseHealth", "blueBaseHealth", "redOutpostHealth", "blueOutpostHealth"}) {
                auto *facility = item(root, name);
                QVERIFY(facility);
                QVERIFY(header.contains(rect(facility)));
            }
            for (const QString &prefix : {QString("red"), QString("blue")}) {
                auto *logo = item(root, prefix + "TeamLogo");
                auto *value = item(item(root, prefix + "BaseHealth"), "baseHealthValue");
                auto *status = item(root, prefix + "BaseStatusText");
                auto *icon = item(root, prefix + "DefenseIcon");
                QVERIFY(logo && value && status && icon);
                QCOMPARE(logo->property("status").toInt(), 1);
                QVERIFY(header.contains(rect(logo)));
                QVERIFY(!rect(logo).intersects(rect(value)));
                QVERIFY(!rect(logo).intersects(rect(item(root, prefix + "TeamName"))));
                QVERIFY(header.contains(rect(status)));
                QVERIFY(icon->isVisible());
                QCOMPARE(status->property("text").toString(), QString("护盾激活"));
            }
            for (int row = 0; row < 8; ++row) {
                auto *marker = item(root, QString("mapRobot%1").arg(ids[row]));
                auto *track = item(marker, "robotHealthTrack");
                auto *fill = item(marker, "robotHealthFill");
                auto *value = item(marker, "robotHealthValue");
                QVERIFY(track && fill && value);
                QVERIFY(rect(track).contains(rect(value)));
                QCOMPARE(value->property("text").toString(), QString::number(health[row]));
                QVERIFY(qAbs(fill->width() / (track->width() - 2) - double(health[row]) / 600) < 0.01);
            }
            QVERIFY(window.grabWindow().save(QString("test-artifacts/hud-%1.png").arg(size.width())));
            const int headerHeight = qCeil(item(root, "matchHeader")->height());
            QVERIFY(window.grabWindow().copy(0, 0, size.width(), headerHeight)
                        .save(QString("test-artifacts/header-%1.png").arg(size.width())));
        }
        // 核对四位前哨血量、满血、摧毁与缺失值，不让数字被组件裁切。
        units.set_base_health(10000); units.set_enemy_base_health(0);
        units.set_outpost_r_health(1000); units.clear_outpost_b_health();
        QVERIFY(send(store, units, now));
        const QStringList facilities{"redBaseHealth", "blueBaseHealth", "redOutpostHealth", "blueOutpostHealth"};
        const QStringList expected{"10000", "0", "1000", "—"};
        for (int i = 0; i < facilities.size(); ++i) {
            auto *facility = item(root, facilities[i]);
            auto *value = item(facility, i < 2 ? "baseHealthValue" : "outpostHealthValue");
            QVERIFY(value);
            QTRY_COMPARE(value->property("text").toString(), expected[i]);
            QVERIFY(value->property("contentWidth").toReal() <= value->width() + 1);
        }
        QVERIFY(!item(root, "blueDefenseIcon")->isVisible());
        QCOMPARE(item(root, "blueBaseStatusText")->property("text").toString(), QString("基地已摧毁"));
        units.clear_base_status();
        QVERIFY(send(store, units, now));
        QCOMPARE(item(root, "redBaseStatusText")->property("text").toString(), QString("状态未知"));
        QVERIFY(!item(root, "redDefenseIcon")->isVisible());
        ly27::wire::RobotStaticStatus missingMaximum;
        missingMaximum.set_robot_id(3); missingMaximum.set_level(2);
        QVERIFY(send(store, missingMaximum, now));
        auto *marker = item(root, "mapRobot3");
        QTRY_COMPARE(item(marker, "robotHealthFill")->width(), 0.0);
        QCOMPARE(item(marker, "robotHealthValue")->property("text").toString(), QString("150"));
        window.setSource(QUrl());
    }

    void economyAndControlPointsRespectIdentityAndFreshness()
    {
        for (const auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            qint64 now = 1000000000;
            ly27::StateStore store(identity(team), nullptr, [&now] { return now; });
            store.reset("resources"); store.setConnected(true);
            ly27::TerminalViewModel model(store, identity(team), "红", "蓝");
            QCOMPARE(model.match()["allyEconomy"].toInt(), -1);
            QCOMPARE(model.match()["redControlPoints"].toInt(), -1);
            QCOMPARE(model.match()["blueControlPoints"].toInt(), -1);
            ly27::wire::GlobalUnitStatus units;
            units.set_total_cp_ally(0); units.set_total_cp_enemy(4294967295U);
            ly27::wire::GlobalLogisticsStatus logistics;
            logistics.set_remaining_economy(0); logistics.set_total_economy_obtained(1000);
            QVERIFY(send(store, units, now)); QVERIFY(send(store, logistics, now));
            const QString allyKey = team == ly27::Team::Red ? "redControlPoints" : "blueControlPoints";
            const QString enemyKey = team == ly27::Team::Red ? "blueControlPoints" : "redControlPoints";
            QCOMPARE(model.match()["allyEconomy"].toInt(), 0);
            QCOMPARE(model.match()[allyKey].toUInt(), 0U);
            QCOMPARE(model.match()[enemyKey].toUInt(), 4294967295U);
            QCOMPARE(model.match()["allyTeamLabel"].toString(), team == ly27::Team::Red ? QString("红方") : QString("蓝方"));
            model.setBlueView(!model.blueView());
            QCOMPARE(model.match()[allyKey].toUInt(), 0U);
            now += 2000000000LL;
            units.set_total_cp_ally(42);
            QVERIFY(send(store, units, now));
            now += 1100000000LL; model.refresh();
            QCOMPARE(model.match()["allyEconomy"].toInt(), -1);
            QCOMPARE(model.match()[allyKey].toUInt(), 42U);
            logistics.set_remaining_economy(245);
            QVERIFY(send(store, logistics, now));
            QCOMPARE(model.match()["allyEconomy"].toInt(), 245);
            store.setConnected(false);
            QCOMPARE(model.match()["allyEconomy"].toInt(), -1);
            QCOMPARE(model.match()[allyKey].toInt(), -1);
            store.setConnected(true);
            QCOMPARE(model.match()["allyEconomy"].toInt(), -1);
            QCOMPARE(model.match()[allyKey].toInt(), -1);
        }
    }

    void headerPresenceAndBluePerspective()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(ly27::Team::Blue), nullptr, [&now] { return now; });
        store.reset("desktop"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(ly27::Team::Blue), "红方测试队", "蓝方测试队");
        QCOMPARE(model.match()["timer"].toString(), QString("--:--"));
        QCOMPARE(model.match()["redBaseHp"].toInt(), -1);
        ly27::wire::GameStatus game;
        game.set_current_round(2); game.set_total_rounds(3); game.set_stage_countdown_sec(61);
        game.set_current_stage(4); game.set_is_paused(true);
        game.set_red_score(0); game.set_blue_score(1);
        QVERIFY(send(store, game, now));
        ly27::wire::GlobalUnitStatus units;
        units.set_base_health(5000); units.set_enemy_base_health(8000);
        units.set_base_status(1); units.set_enemy_base_status(2);
        units.set_outpost_r_health(0); units.set_outpost_b_health(700);
        QVERIFY(send(store, units, now));
        QCOMPARE(model.match()["timer"].toString(), QString("01:01"));
        QCOMPARE(model.match()["redScore"].toInt(), 0);
        QCOMPARE(model.match()["currentRound"].toInt(), 2);
        QCOMPARE(model.match()["redBaseHp"].toInt(), 8000);
        QCOMPARE(model.match()["blueBaseHp"].toInt(), 5000);
        QCOMPARE(model.match()["redBaseStatusText"].toString(), QString("护甲展开"));
        QCOMPARE(model.match()["blueBaseStatusText"].toString(), QString("护甲未展开"));
        QCOMPARE(model.match()["rOutpostHp"].toInt(), 0);
        QCOMPARE(model.match()["bOutpostHp"].toInt(), 700);
        QCOMPARE(model.match()["redTeam"].toString(), QString("红方测试队"));
        QVERIFY(model.match()["paused"].toBool());
    }

    void allRobotIdsHealthLevelsAndProjection()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("robots"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(), "红", "蓝");
        auto *robots = model.robots();
        QCOMPARE(robots->rowCount(), 8);
        const int ids[] = {1, 3, 4, 7, 101, 103, 104, 107};
        ly27::wire::GlobalUnitStatus units;
        for (int i = 0; i < 8; ++i) units.add_robot_health(i == 0 ? 0 : 100 + i);
        QVERIFY(send(store, units, now));
        for (int row = 0; row < 8; ++row) {
            ly27::wire::RobotPosition position;
            position.set_robot_id(ids[row]); position.set_x(row * 3); position.set_y(row + 1);
            position.set_yaw(45);
            QVERIFY(send(store, position, now));
            ly27::wire::RobotStaticStatus stat;
            stat.set_robot_id(ids[row]); stat.set_level(row + 1); stat.set_max_health(400);
            QVERIFY(send(store, stat, now));
        }
        for (int row = 0; row < 8; ++row) {
            const auto index = robots->index(row, 0);
            QCOMPARE(robots->data(index, ly27::RobotMapModel::RobotId).toInt(), ids[row]);
            QCOMPARE(robots->data(index, ly27::RobotMapModel::HpText).toString(),
                     QString::number(row == 0 ? 0 : 100 + row));
            QCOMPARE(robots->data(index, ly27::RobotMapModel::LevelText).toString(),
                     QString("Lv.%1").arg(row + 1));
            QVERIFY(robots->data(index, ly27::RobotMapModel::PositionKnown).toBool());
            const auto expected = ly27::RobotMapModel::projectPosition(row * 3, row + 1);
            QCOMPARE(robots->data(index, ly27::RobotMapModel::MapX).toDouble(), expected.x());
            QCOMPARE(robots->data(index, ly27::RobotMapModel::MapY).toDouble(), expected.y());
        }
        const auto index = robots->index(1, 0);
        const double x = robots->data(index, ly27::RobotMapModel::MapX).toDouble();
        const double y = robots->data(index, ly27::RobotMapModel::MapY).toDouble();
        QVERIFY(robots->data(index, ly27::RobotMapModel::Self).toBool());
        model.setBlueView(true);
        QCOMPARE(robots->data(index, ly27::RobotMapModel::MapX).toDouble(), 1.0 - x);
        QCOMPARE(robots->data(index, ly27::RobotMapModel::MapY).toDouble(), 1.0 - y);
        const auto bottomLeft = ly27::RobotMapModel::projectPosition(0, 0);
        const auto topRight = ly27::RobotMapModel::projectPosition(28, 15);
        QVERIFY(bottomLeft.x() < topRight.x());
        QVERIFY(bottomLeft.y() > topRight.y());
        QCOMPARE(ly27::RobotMapModel::projectPosition(-10, -5), bottomLeft);
        QCOMPARE(ly27::RobotMapModel::projectPosition(40, 30), topRight);
    }

    void positionExpirationDisconnectRetainedAndNewRound()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("expiry"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(), "红", "蓝");
        auto *robots = model.robots();
        auto known = [&] { return robots->data(robots->index(1, 0), ly27::RobotMapModel::PositionKnown).toBool(); };
        ly27::wire::GameStatus game; game.set_current_round(1);
        QVERIFY(send(store, game, now));
        ly27::wire::RobotPosition position;
        position.set_robot_id(3); position.set_x(0); position.set_y(0);
        QVERIFY(send(store, position, now));
        QVERIFY(known());
        now += 3100000000LL; model.refresh();
        QVERIFY(known());
        QVERIFY(!robots->data(robots->index(1, 0), ly27::RobotMapModel::PositionFresh).toBool());
        now += 2000000000LL; model.refresh();
        QVERIFY(!known());
        QVERIFY(send(store, position, now));
        store.setConnected(false); QVERIFY(!known());
        store.setConnected(true); QVERIFY(!known());
        QVERIFY(send(store, position, now, true)); QVERIFY(!known());
        QVERIFY(send(store, position, now)); QVERIFY(known());
        game.set_current_round(2);
        QVERIFY(send(store, game, now));
        QVERIFY(!known());
    }

    void qmlRendersAndTracksActualImageArea()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("render"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(), "红方测试队", "蓝方测试队");
        ly27::wire::RobotPosition position;
        position.set_robot_id(3); position.set_x(14); position.set_y(7.5);
        QVERIFY(send(store, position, now));
        ly27::MapWindow window(&model);
        QCOMPARE(window.status(), QQuickView::Ready);
        window.show();
        auto *root = window.rootObject();
        auto *map = item(root, "battlefieldImage");
        auto *layer = item(root, "mapMarkerLayer");
        auto *marker = item(root, "mapRobot3");
        QVERIFY(map && layer && marker);
        for (const QSize size : {QSize(1280, 800), QSize(640, 900)}) {
            window.resize(size);
            QTRY_COMPARE(layer->width(), map->property("paintedWidth").toDouble());
            QTRY_COMPARE(layer->height(), map->property("paintedHeight").toDouble());
            QTRY_VERIFY(marker->isVisible());
            const auto expected = ly27::RobotMapModel::projectPosition(14, 7.5);
            QTRY_VERIFY(qAbs(marker->x() - expected.x() * layer->width()) < 0.5);
            QTRY_VERIFY(qAbs(marker->y() - expected.y() * layer->height()) < 0.5);
            QVERIFY(!window.grabWindow().isNull());
        }
        window.setSource(QUrl());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    DesktopTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_desktop.moc"
