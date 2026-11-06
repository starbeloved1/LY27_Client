#include "BuffPresentation.hpp"
#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QGuiApplication>
#include <QPointer>
#include <QQuickItem>
#include <QSignalSpy>
#include <QtTest>

namespace {
ly27::TerminalIdentity identity()
{
    ly27::TerminalIdentity id; id.role = ly27::Role::Commander; id.stationId = "buff-test"; return id;
}
bool send(ly27::StateStore &store, const google::protobuf::Message &message, qint64 &now, bool retained = false)
{
    ++now;
    return store.apply(ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
        QByteArray::fromStdString(message.SerializeAsString())), 1700000000000, now, retained);
}
ly27::wire::Buff buff(int id, int type, int level, int seconds = 20)
{
    ly27::wire::Buff b; b.set_robot_id(id); b.set_buff_type(type); b.set_buff_level(level);
    b.set_buff_max_time(seconds); b.set_buff_left_time(seconds); return b;
}
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
}

class BuffsTest : public QObject {
    Q_OBJECT
private slots:
    void typedFacilityBuffsAndLifecycle()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&] { return now; }); store.reset("buffs"); store.setConnected(true);
        for (int id : {3, 10, 11, 110, 111}) {
            QVERIFY(send(store, buff(id, 2, 50, 1), now));
            const auto states = store.buffs(id);
            QCOMPARE(states.size(), 1); QVERIFY(states.value(ly27::BuffType::Defense).level.isFresh());
            QCOMPARE(ly27::mapBuffs(states).size(), 1);
            if (id != 3) QVERIFY(!store.robot(id)); // 建筑不能混入地面机器人集合。
        }
        QVERIFY(send(store, buff(10, 5, 10, 4), now));
        QSignalSpy changes(&store, &ly27::StateStore::changed);
        now += 1000000010LL; store.refresh(); QVERIFY(changes.count() > 0);
        QVERIFY(!store.buff(10, 2).isFresh()); QVERIFY(store.buff(10, 5).isFresh());
        QCOMPARE(ly27::mapBuffs(store.buffs(10)).size(), 1);
        QCOMPARE(ly27::mapBuffs(store.buffs(10)).first().toMap()["type"].toInt(), 5);
        QVERIFY(send(store, buff(10, 5, 10, 0), now)); QVERIFY(ly27::mapBuffs(store.buffs(10)).isEmpty());
        QVERIFY(send(store, buff(999, 2, 50), now)); QVERIFY(store.buffs(999).isEmpty());
        QVERIFY(send(store, buff(10, 2, 50), now, true)); QVERIFY(ly27::mapBuffs(store.buffs(10)).isEmpty());
        QVERIFY(send(store, buff(10, 2, 50), now)); store.setConnected(false);
        QVERIFY(ly27::mapBuffs(store.buffs(10)).isEmpty());
        store.setConnected(true); QVERIFY(ly27::mapBuffs(store.buffs(10)).isEmpty());
        QVERIFY(send(store, buff(10, 2, 50), now));
        ly27::wire::GameStatus game; game.set_current_round(1); QVERIFY(send(store, game, now));
        game.set_current_round(2); QVERIFY(send(store, game, now)); QVERIFY(store.buffs(10).isEmpty());
        QVERIFY(send(store, buff(111, 2, -40), now)); store.reset("next"); QVERIFY(store.buffs(111).isEmpty());
    }
    void actualValuesAndInvalidFields()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&] { return now; }); store.reset("values"); store.setConnected(true);
        for (int type = 1; type <= 7; ++type) QVERIFY(send(store, buff(3, type, type == 2 ? -37 : 17), now));
        auto views = ly27::mapBuffs(store.buffs(3)); QCOMPARE(views.size(), 7);
        QCOMPARE(views[1].toMap()["level"].toLongLong(), -37LL);
        QCOMPARE(views[1].toMap()["icon"].toString(), QString("image://buff/vulnerability"));
        QVERIFY(views[1].toMap()["description"].toString().contains("37%"));
        QVERIFY(views[2].toMap()["description"].toString().contains("17/s"));
        QCOMPARE(views[0].toMap()["icon"].toString(), QString("image://buff/attack"));
        for (auto invalid : {buff(3, 1, 0), buff(3, 1, -10), buff(3, 1, 50, 0)}) {
            QVERIFY(send(store, invalid, now)); QCOMPARE(ly27::mapBuffs(store.buffs(3)).size(), 6);
        }
        auto missing = buff(3, 1, 50); missing.clear_buff_left_time(); QVERIFY(send(store, missing, now));
        QCOMPARE(ly27::mapBuffs(store.buffs(3)).size(), 6);
        missing = buff(3, 1, 50); missing.clear_buff_level(); QVERIFY(send(store, missing, now));
        QCOMPARE(ly27::mapBuffs(store.buffs(3)).size(), 6);
    }
    void operatorHasBuffsWithoutPosition()
    {
        auto id = identity(); id.role = ly27::Role::Infantry; id.robotId = 3;
        qint64 now = 1000000000;
        ly27::StateStore store(id, nullptr, [&] { return now; }); store.reset("operator"); store.setConnected(true);
        ly27::TerminalViewModel model(store, id, "红方", "蓝方");
        ly27::wire::GameStatus game; game.set_current_stage(4); game.set_is_paused(false); QVERIFY(send(store, game, now));
        QVERIFY(send(store, buff(3, 2, 25), now)); QVERIFY(send(store, buff(4, 1, 50), now));
        QCOMPARE(model.selfBuffs().size(), 1); QCOMPARE(model.selfBuffs().first().toMap()["type"].toInt(), 2);
        ly27::MapWindow window(&model); window.show(); QCOMPARE(window.status(), QQuickView::Ready);
        QTest::qWait(100); auto *root = window.rootObject();
        QVERIFY(item(root, "selfBuffPanel")->isVisible()); QVERIFY(!item(root, "mapRobot3")->isVisible());
        ly27::wire::RobotPosition p; p.set_robot_id(3); p.set_x(10); p.set_y(7); QVERIFY(send(store, p, now));
        QTest::qWait(100);
        QVERIFY(item(root, "mapRobot3")->isVisible());
        QVERIFY(item(item(root, "mapRobot3"), "robotBuffRow")->isVisible());
        QDir().mkpath("test-artifacts"); QVERIFY(window.grabWindow().save("test-artifacts/map-buffs-operator.png"));
        store.setConnected(false); QVERIFY(model.selfBuffs().isEmpty()); QVERIFY(!item(root, "selfBuffPanel")->isVisible());
        window.setSource(QUrl());
    }
    void mapRowsAndMovingDefenseKeepTheirItems()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&] { return now; }); store.reset("display"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(), "红方", "蓝方");
        ly27::wire::GameStatus game; game.set_current_stage(4); game.set_current_round(1);
        game.set_stage_countdown_sec(300); game.set_is_paused(false); QVERIFY(send(store, game, now));
        ly27::wire::GlobalUnitStatus units; units.set_base_health(8000); units.set_enemy_base_health(9000);
        units.set_outpost_r_health(700); units.set_outpost_b_health(800);
        for (int i = 0; i < 8; ++i) units.add_robot_health(200);
        QVERIFY(send(store, units, now));
        ly27::wire::RobotPosition position;
        for (int id : {3, 4, 103}) {
            position.set_robot_id(id); position.set_x(id == 3 ? 10 : id == 4 ? 5 : 27.8);
            position.set_y(id == 103 ? 14.8 : 7); QVERIFY(send(store, position, now));
            ly27::wire::RobotStaticStatus stat; stat.set_robot_id(id); stat.set_max_health(400); stat.set_level(2);
            QVERIFY(send(store, stat, now));
        }
        for (int type = 1; type <= 7; ++type) QVERIFY(send(store, buff(3, type, type == 2 ? -30 : 50), now));
        QVERIFY(send(store, buff(4, 2, 25), now));
        for (int type = 1; type <= 7; ++type) QVERIFY(send(store, buff(103, type, 10), now));
        QVERIFY(send(store, buff(10, 2, 50), now)); QVERIFY(send(store, buff(11, 5, 10), now));
        QCOMPARE(model.facilityBuffs()["110"].toList().size(), 0);
        ly27::MapWindow window(&model); window.show(); QCOMPARE(window.status(), QQuickView::Ready);
        auto *root = window.rootObject(); auto *layer = item(root, "mapMarkerLayer"); QVERIFY(layer);
        QTest::qWait(120);
        for (const auto &name : {"mapRobot3", "mapRobot4", "mapRobot103", "mapRedBase", "mapROutpost"}) {
            auto *badge = item(root, name); QVERIFY(badge);
            const bool building = QString(name).contains("Base") || QString(name).contains("Outpost");
            auto *row = item(badge, building ? "facilityBuffRow" : "robotBuffRow"); QVERIFY(row && row->isVisible());
            auto *caption = item(badge, building ? "facilityMapCaption" : "robotHealthCaption"); QVERIFY(caption);
            QCOMPARE(row->y() + row->height() + 2, caption->y());
            if (!building) {
                QCOMPARE(row->x(), caption->x()); // 图标左边与等级格对齐。
                if (caption->y() < 0) QCOMPARE(caption->y(), -40.0); // 沿用原血条/车标间距。
                else QVERIFY(row->y() >= 19); // 地图上边缘时整组放到车标下方。
            }
            for (int type = 1; type <= 7; ++type) {
                auto *icon = item(row, "mapBuff" + QString::number(type)); QVERIFY(icon);
                if (icon->isVisible()) QTRY_COMPARE(item(icon, "mapBuffImage")->property("status").toInt(), 1);
            }
        }
        QDir().mkpath("test-artifacts");
        for (bool blue : {false, true}) {
            model.setBlueView(blue);
            for (auto size : {QSize(1280, 800), QSize(640, 480)}) {
                window.resize(size); QTest::qWait(120);
                for (const auto &name : {"mapRobot3", "mapRobot4", "mapRobot103", "mapRedBase", "mapROutpost"}) {
                    auto *badge = item(root, name);
                    auto *row = item(badge, QString(name).contains("Robot") ? "robotBuffRow" : "facilityBuffRow");
                    QVERIFY2(QRectF(QPointF(), layer->size()).contains(row->mapRectToItem(layer, QRectF(QPointF(), row->size()))), name);
                    auto *caption = item(badge, QString(name).contains("Robot") ? "robotHealthCaption" : "facilityMapCaption");
                    QCOMPARE(row->x(), caption->x());
                }
                QVERIFY(window.grabWindow().save(QString("test-artifacts/map-buffs-%1-%2.png").arg(blue ? "blue" : "red").arg(size.width())));
            }
        }
        model.setBlueView(false); window.resize(1280, 800); QTest::qWait(100);
        ly27::wire::MapClickInfo message; message.set_is_send_all(1);
        message.set_robot_id(std::string(1, char(3)) + std::string(6, '\0'));
        message.set_mode(2); message.set_type(2); message.set_enemy_id(4); QVERIFY(send(store, message, now));
        QTest::qWait(100);
        QPointer<QQuickItem> mark = item(root, "tacticalMarker3"); QVERIFY(mark && mark->isVisible());
        QPointer<QQuickItem> markIcon = item(mark, "tacticalMarkerBadge"); QVERIFY(markIcon);
        QPointer<QQuickItem> buffIcon = item(item(root, "mapRobot4"), "mapBuff2"); QVERIFY(buffIcon);
        QSignalSpy destroyedMark(mark, &QObject::destroyed), destroyedBuff(buffIcon, &QObject::destroyed);
        position.set_robot_id(4); position.set_y(7);
        for (int i = 0; i < 20; ++i) {
            now += 50000000; position.set_x(5 + i * .1f); QVERIFY(send(store, position, now));
            QTest::qWait(12);
            QVERIFY(mark && markIcon && buffIcon && mark->isVisible());
            QCOMPARE(item(root, "tacticalMarker3"), mark.data());
            QCOMPARE(item(mark, "tacticalMarkerBadge"), markIcon.data());
            QVERIFY(mark->x() > layer->width() * .1); // 不重新从零坐标开始跟随动画。
        }
        QCOMPARE(destroyedMark.count(), 0); QCOMPARE(destroyedBuff.count(), 0);
        QTest::qWait(100);
        const auto point = ly27::RobotMapModel::projectPosition(position.x(), position.y());
        QVERIFY(qAbs(mark->x() - point.x() * layer->width()) < 1);
        QVERIFY(mark->property("lift").toDouble() > 64); // 标记避开新增加的 Buff 行。
        QVERIFY(window.grabWindow().save("test-artifacts/map-buffs-stable-defense.png"));
        now += 20000000000LL; store.refresh(); model.refresh(); QTest::qWait(40);
        QVERIFY(!item(item(root, "mapRedBase"), "facilityBuffRow")->isVisible());
        QVERIFY(!item(item(root, "mapRobot3"), "robotBuffRow")->isVisible());
        window.setSource(QUrl());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv); BuffsTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_buffs.moc"
