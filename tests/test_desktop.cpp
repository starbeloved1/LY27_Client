#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QGuiApplication>
#include <QDir>
#include <QQuickItem>
#include <QQuickImageProvider>
#include <QQmlEngine>
#include <algorithm>
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
    void battlefieldToneIsCachedAndKeepsProjectionSize()
    {
        ly27::StateStore store(identity());
        ly27::TerminalViewModel model(store, identity(), "红", "蓝");
        ly27::MapWindow window(&model);
        auto *provider = dynamic_cast<QQuickImageProvider *>(window.engine()->imageProvider("battlefield"));
        QVERIFY(provider);
        QSize size;
        const auto image = provider->requestImage("map", &size, {});
        const QImage original(":/ly27/ui/maps/battlefield.png");
        QVERIFY(!image.isNull()); QCOMPARE(size, original.size()); QCOMPARE(image.size(), original.size());
        QCOMPARE(provider->requestImage("map", nullptr, {}).cacheKey(), image.cacheKey());
        QVERIFY(provider->requestImage("unknown", nullptr, {}).isNull());
        qint64 brightnessBefore = 0, brightnessAfter = 0, chromaBefore = 0, chromaAfter = 0;
        for (int y = 0; y < image.height(); y += 23) for (int x = 0; x < image.width(); x += 23) {
            const auto before = original.pixelColor(x, y), after = image.pixelColor(x, y);
            QCOMPARE(before.alpha(), after.alpha());
            const auto brightness = [](const QColor &c) { return c.red() * 54 + c.green() * 183 + c.blue() * 19; };
            const auto chroma = [](const QColor &c) { return std::max({c.red(), c.green(), c.blue()}) - std::min({c.red(), c.green(), c.blue()}); };
            brightnessBefore += brightness(before); brightnessAfter += brightness(after);
            chromaBefore += chroma(before); chromaAfter += chroma(after);
        }
        QVERIFY(brightnessAfter < brightnessBefore * .9);
        QVERIFY(chromaBefore > 0); QVERIFY(chromaAfter < chromaBefore * .7);
        auto *frame = item(window.rootObject(), "mapContrastVeil"); QVERIFY(frame);
        QCOMPARE(frame->property("color").value<QColor>().alpha(), 0);
        window.setSource(QUrl());
    }
    void commanderSelectionRetainsTeamColorAndHealthCenter()
    {
        for (auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            auto id = identity(team); id.role = ly27::Role::Commander; id.robotId.reset();
            const int receiver = team == ly27::Team::Red ? 3 : 103;
            qint64 now = 1000000000;
            ly27::StateStore store(id, nullptr, [&] { return now; }); store.reset("marker-style"); store.setConnected(true);
            ly27::TerminalViewModel model(store, id, "红", "蓝");
            ly27::wire::GameStatus g; g.set_current_round(1); g.set_current_stage(4); g.set_is_paused(false);
            QVERIFY(send(store, g, now));
            ly27::wire::RobotPosition p; p.set_robot_id(receiver); p.set_x(12); p.set_y(7); p.set_yaw(45);
            QVERIFY(send(store, p, now));
            ly27::wire::RobotStaticStatus stat; stat.set_robot_id(receiver); stat.set_max_health(400); stat.set_level(10);
            QVERIFY(send(store, stat, now));
            ly27::MapWindow window(&model); window.show(); QTest::qWait(60);
            auto *robot = item(window.rootObject(), "mapRobot" + QString::number(receiver)); QVERIFY(robot);
            auto *caption = item(robot, "robotHealthCaption"); QVERIFY(caption);
            auto *track = item(robot, "robotHealthTrack"); QVERIFY(track);
            auto *badge = item(robot, "robotNumberBadge"); QVERIFY(badge);
            const auto fill = robot->property("markerColor").value<QColor>();
            const auto badgeColor = badge->property("color").value<QColor>();
            // 底图加载后检查血槽本体的中心，不能把左侧等级格算进居中宽度。
            QTRY_COMPARE(track->mapRectToItem(robot, QRectF(QPointF(), track->size())).center().x(), 0.0);
            QVERIFY(caption->width() < badge->width() * 1.6); // 控制横向占位，不用长横杠压过棋子。
            QVERIFY(item(robot, "robotHeadingWedge"));
            model.tactics()->clickRobot(receiver);
            QVERIFY(robot->property("selected").toBool());
            QVERIFY(item(robot, "robotSelectionRing")->isVisible());
            QCOMPARE(robot->property("markerColor").value<QColor>(), fill);
            QCOMPARE(badge->property("color").value<QColor>(), badgeColor);
            QVERIFY(!item(robot, "robotSelfRing")->isVisible());
            QDir().mkpath("test-artifacts");
            QTest::qWait(60);
            QVERIFY(window.grabWindow().save(QString("test-artifacts/selection-keeps-team-%1.png").arg(receiver)));
            model.tactics()->cancel(); QVERIFY(!item(robot, "robotSelectionRing")->isVisible());
            QCOMPARE(robot->property("markerColor").value<QColor>(), fill);
            window.setSource(QUrl());
        }
    }
    void baseHealthLengthMatchesNumericValue()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("base-health-ratio"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(), "红方", "蓝方");
        ly27::MapWindow window(&model); window.show();
        auto *root = window.rootObject();
        ly27::wire::GlobalUnitStatus units;
        for (int hp : {10000, 7800, 6000, 5000, 4000, 1000, 0, 5000, 10000}) {
            units.set_base_health(hp); units.set_enemy_base_health(hp);
            QVERIFY(send(store, units, now));
            for (const auto &prefix : {QString("red"), QString("blue")}) {
                auto *bar = item(root, prefix + "BaseHealth"); QVERIFY(bar);
                auto *fill = item(bar, "baseHealthFill"); QVERIFY(fill);
                auto *frame = item(bar, "baseHealthFrame"); QVERIFY(frame);
                QCOMPARE(frame->width(), bar->width());
                auto *value = item(bar, "baseHealthValue"); QVERIFY(value);
                QCOMPARE(value->property("text").toString(), QString::number(hp));
                QVERIFY(qAbs(fill->width() / bar->width() - hp / 10000.0) < 0.0001);
                QCOMPARE(fill->x(), prefix == "blue" ? 0.0 : bar->width() - fill->width());
                auto *map = item(root, prefix == "blue" ? "mapBlueBase" : "mapRedBase");
                QCOMPARE(map->property("ratio").toDouble(), hp / 10000.0);
            }
            if (hp == 5000) {
                QTest::qWait(300);
                auto *bar = item(root, "redBaseHealth");
                QTRY_COMPARE(bar->property("trailHealth").toInt(), hp);
                QVERIFY(QDir().mkpath("test-artifacts"));
                QVERIFY(window.grabWindow().save("test-artifacts/base-half-health.png"));
            }
        }
        units.clear_base_health(); units.clear_enemy_base_health();
        QVERIFY(send(store, units, now));
        for (const auto &prefix : {QString("red"), QString("blue")}) {
            auto *bar = item(root, prefix + "BaseHealth");
            QCOMPARE(item(bar, "baseHealthValue")->property("text").toString(), QString("—"));
            QCOMPARE(item(bar, "baseHealthFill")->width(), 0.0);
            QCOMPARE(bar->property("trailHealth").toInt(), 0);
        }
        window.setSource(QUrl());
    }
    void mapFacilityHealthAndPerspective()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(ly27::Team::Blue), nullptr, [&now] { return now; });
        store.reset("facilities"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(ly27::Team::Blue), "红方", "蓝方");
        ly27::MapWindow window(&model);
        QCOMPARE(window.status(), QQuickView::Ready);
        window.show();
        auto *root = window.rootObject();
        QVERIFY(QDir().mkpath("test-artifacts"));
        ly27::wire::GlobalUnitStatus units;
        units.set_base_health(6789); units.set_enemy_base_health(8321);
        units.set_outpost_r_health(765); units.set_outpost_b_health(0);
        units.set_outpost_r_status(4); units.set_outpost_b_status(1);
        QVERIFY(send(store, units, now));
        const QStringList names{"RedBase", "BlueBase", "ROutpost", "BOutpost"};
        const QStringList expected{"8321", "6789", "765", "0"};
        const QPointF centers[] = {{171, 461}, {1514, 464}, {667, 685}, {1028, 237}};
        for (const bool blueView : {false, true}) {
            model.setBlueView(blueView);
            for (const QSize size : {QSize(1920, 1080), QSize(1280, 800), QSize(640, 900), QSize(640, 480)}) {
                window.resize(size); QTest::qWait(200);
                auto *layer = item(root, "mapMarkerLayer");
                QVERIFY(layer);
                for (int i = 0; i < names.size(); ++i) {
                    auto *facility = item(root, "map" + names[i]);
                    QVERIFY(facility);
                    auto *caption = item(facility, "facilityMapCaption");
                    auto *track = item(facility, "facilityMapHealthTrack");
                    auto *fill = item(facility, "facilityMapHealthFill");
                    auto *unknown = item(facility, "facilityMapHealthUnknown");
                    QVERIFY(caption && track && fill && unknown);
                    QVERIFY(!unknown->isVisible());
                    const double maximum = i >= 2 ? 1000 : 10000;
                    QVERIFY(qAbs(fill->width() / (track->width() - 2) - expected[i].toDouble() / maximum) < 0.01);
                    if (i >= 2) {
                        auto *icon = item(facility, "facilityMapIcon");
                        QVERIFY(icon);
                        QVERIFY(icon->isVisible());
                        QCOMPARE(icon->width(), facility->property("tiny").toBool()
                            ? qMin(33.0, layer->height() * .20) : 62.0);
                        QTRY_COMPARE(icon->property("status").toInt(), 1);
                        QCOMPARE(icon->property("source").toUrl().fileName(),
                                 i == 2 ? QString("red_map_outpost.png") : QString("unknown_map_outpost.png"));
                        const auto iconBounds = icon->mapRectToItem(layer, QRectF(QPointF(), icon->size()));
                        QVERIFY(QRectF(0, 0, layer->width(), layer->height()).contains(iconBounds));
                    }
                    const double u = centers[i].x() / 1662, v = centers[i].y() / 946;
                    QVERIFY(qAbs(facility->x() - (blueView ? 1 - u : u) * layer->width()) < 0.5);
                    QVERIFY(qAbs(facility->y() - (blueView ? 1 - v : v) * layer->height()) < 0.5);
                    const auto bounds = caption->mapRectToItem(layer, QRectF(QPointF(), caption->size()));
                    QVERIFY(QRectF(0, 0, layer->width(), layer->height()).contains(bounds));
                    QVERIFY2(bounds.bottom() <= facility->y() - facility->property("footprintRadius").toReal() + 0.5,
                        qPrintable(QString("%1 at %2x%3, blue=%4").arg(names[i]).arg(size.width()).arg(size.height()).arg(blueView)));
                }
                QVERIFY(window.grabWindow().save(QString("test-artifacts/facilities-%1-%2x%3.png")
                    .arg(blueView ? "blue" : "red").arg(size.width()).arg(size.height())));
            }
        }
        units.set_base_health(0); units.clear_enemy_base_health();
        units.clear_outpost_r_health(); units.set_outpost_b_health(1000);
        QVERIFY(send(store, units, now));
        QTRY_COMPARE(item(item(root, "mapBlueBase"), "facilityMapHealthFill")->width(), 0.0);
        QVERIFY(!item(item(root, "mapBlueBase"), "facilityMapHealthUnknown")->isVisible());
        QVERIFY(item(item(root, "mapRedBase"), "facilityMapHealthUnknown")->isVisible());
        QVERIFY(item(item(root, "mapROutpost"), "facilityMapHealthUnknown")->isVisible());
        auto *outpost = item(root, "mapBOutpost");
        QVERIFY(!item(outpost, "facilityMapHealthUnknown")->isVisible());
        auto *track = item(outpost, "facilityMapHealthTrack");
        QTRY_COMPARE(item(outpost, "facilityMapHealthFill")->width(), track->width() - 2);
        now += 3100000000LL; model.refresh();
        QVERIFY(item(outpost, "facilityMapHealthUnknown")->isVisible());
        window.setSource(QUrl());
    }

    void hudLayoutAndHealthBars()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("hud"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(), "凌云战队 · 红方测试队名", "蓝方测试战队");
        ly27::wire::GameStatus game;
        game.set_current_round(2); game.set_total_rounds(3); game.set_stage_countdown_sec(287);
        game.set_current_stage(4); game.set_is_paused(false); game.set_red_score(1); game.set_blue_score(0);
        QVERIFY(send(store, game, now));
        ly27::wire::GlobalUnitStatus units;
        units.set_base_health(8321); units.set_enemy_base_health(6789);
        units.set_base_status(0); units.set_enemy_base_status(0);
        units.set_outpost_r_health(765); units.set_outpost_b_health(0);
        units.set_total_cp_ally(1250); units.set_total_cp_enemy(980);
        units.set_total_damage_ally(12345678); units.set_total_damage_enemy(98765432);
        const int health[] = {0, 150, 300, 450, 600, 500, 120, 350};
        const int ids[] = {1, 3, 4, 7, 101, 103, 104, 107};
        for (int hp : health) units.add_robot_health(hp);
        QVERIFY(send(store, units, now));
        ly27::wire::GlobalLogisticsStatus logistics;
        logistics.set_remaining_economy(550);
        logistics.set_total_economy_obtained(1550);
        logistics.set_tech_level(4); logistics.set_encryption_level(3);
        QVERIFY(send(store, logistics, now));
        for (int row = 0; row < 8; ++row) {
            ly27::wire::RobotPosition position;
            position.set_robot_id(ids[row]); position.set_x(2 + row * 3); position.set_y(2 + row * 1.4);
            QVERIFY(send(store, position, now));
            ly27::wire::RobotStaticStatus stat;
            stat.set_robot_id(ids[row]); stat.set_level(row + 1); stat.set_max_health(600);
            QVERIFY(send(store, stat, now));
        }
        const QList<QPair<int, QString>> notices{{1, "1,101"}, {2, "111"}, {5, "2"}, {6, "4"},
            {9, "1,3"}, {11, ""}, {15, "7"}, {16, ""}};
        for (const auto &notice : notices) {
            ly27::wire::Event event; event.set_event_id(notice.first); event.set_param(notice.second.toStdString());
            QVERIFY(send(store, event, now));
        }
        ly27::MapWindow window(&model);
        QCOMPARE(window.status(), QQuickView::Ready);
        window.show();
        auto *root = window.rootObject();
        auto rect = [](QQuickItem *i) { return i->mapRectToScene(QRectF(QPointF(), i->size())); };
        const QStringList names{"matchRound", "matchRedScore", "matchTimer", "matchBlueScore", "matchStage",
                                "redTeamName", "blueTeamName", "allyEconomyLabel", "allyEconomyValue",
                                "allyTotalEconomyLabel", "allyTotalEconomyValue",
                                "redControlPointsLabel", "redControlPointsValue", "blueControlPointsLabel", "blueControlPointsValue",
                                "redDamageLabel", "redDamageValue", "blueDamageLabel", "blueDamageValue",
                                "allyTechLevelLabel", "allyTechLevelValue", "allyEncryptionLevelLabel", "allyEncryptionLevelValue",
                                "ControlPointsNetRate"};
        QVERIFY(QDir().mkpath("test-artifacts"));
        for (const QSize size : {QSize(2560, 1080), QSize(1920, 1080), QSize(1280, 800), QSize(1050, 700),
                                 QSize(1024, 768), QSize(640, 900), QSize(640, 480)}) {
            window.resize(size);
            QTest::qWait(220);
            const auto header = rect(item(root, "matchHeader"));
            auto *panel = item(root, "eventPanel");
            auto *mapArea = item(root, "mapArea");
            auto *events = item(root, "eventList");
            auto *analysis = item(root, "battleAnalysisPanel");
            auto *map = item(root, "battlefieldImage");
            auto *layer = item(root, "mapMarkerLayer");
            QVERIFY(panel && mapArea && events && analysis && map && layer);
            auto *stage = item(root, "matchStage"); QVERIFY(stage);
            QVERIFY(rect(stage).left() < 24 && rect(stage).top() < 12);
            QCOMPARE(stage->property("color").value<QColor>(), QColor(Qt::white));
            QVERIFY(rect(panel).right() <= rect(mapArea).left());
            QCOMPARE(rect(panel).top(), header.bottom());
            QCOMPARE(rect(panel).bottom(), rect(analysis).top());
            QCOMPARE(rect(analysis).left(), 0.0);
            QCOMPARE(rect(analysis).right(), double(window.width()));
            QCOMPARE(rect(analysis).bottom(), double(window.height()));
            QCOMPARE(rect(mapArea).top(), header.bottom());
            QCOMPARE(rect(mapArea).bottom(), rect(analysis).top());
            QVERIFY(!rect(mapArea).intersects(rect(analysis)));
            QVERIFY(!rect(panel).intersects(rect(analysis)));
            // 保留底图比例，并确认图片已经填满可用宽或高，没有额外缩小地图。
            const double imageWidth = map->property("paintedWidth").toDouble();
            const double imageHeight = map->property("paintedHeight").toDouble();
            QVERIFY(qAbs(imageWidth / imageHeight - 1662.0 / 946.0) < 0.001);
            QVERIFY(qAbs(imageWidth - mapArea->width()) < 0.5);
            QVERIFY(qAbs(imageHeight - mapArea->height()) < 0.5);
            QVERIFY(qAbs(mapArea->width() / mapArea->height() - 1662.0 / 946.0) < 0.001);
            QVERIFY(rect(mapArea).left() > rect(panel).right());
            QCOMPARE(events->property("count").toInt(), 8);
            QTRY_VERIFY2(events->property("atYEnd").toBool(), qPrintable(QString("%1x%2").arg(size.width()).arg(size.height())));
            for (int a = 0; a < names.size(); ++a) {
                auto *text = item(root, names[a]);
                QVERIFY(text);
                const bool resource = names[a].contains("Economy") || names[a].contains("ControlPoints") ||
                    names[a].contains("Damage") || names[a].contains("TechLevel") || names[a].contains("EncryptionLevel");
                QVERIFY2((resource ? rect(analysis) : header).contains(rect(text)), qPrintable(names[a]));
                if (!resource)
                    QVERIFY2(text->property("contentHeight").toReal() <= text->height() + 1, qPrintable(names[a]));
                if (!names[a].contains("TeamName"))
                    QVERIFY2(text->property("contentWidth").toReal() <= text->width() + 1, qPrintable(names[a]));
                for (int b = a + 1; b < names.size(); ++b)
                    QVERIFY2(!rect(text).intersects(rect(item(root, names[b]))), qPrintable(names[a] + " / " + names[b]));
            }
            QCOMPARE(item(root, "matchTimer")->property("text").toString(), QString("04:47"));
            QCOMPARE(item(root, "matchStage")->property("text").toString(), QString("比赛进行中"));
            QCOMPARE(item(root, "allyEconomyValue")->property("text").toString(), QString("550"));
            QCOMPARE(item(root, "allyTotalEconomyValue")->property("text").toString(), QString("1550"));
            QCOMPARE(item(root, "redControlPointsValue")->property("text").toString(), QString("1250"));
            QCOMPARE(item(root, "blueControlPointsValue")->property("text").toString(), QString("980"));
            QCOMPARE(item(root, "redDamageValue")->property("text").toString(), QString("12345678"));
            QCOMPARE(item(root, "blueDamageValue")->property("text").toString(), QString("98765432"));
            QCOMPARE(item(root, "allyTechLevelValue")->property("text").toString(), QString("4"));
            QCOMPARE(item(root, "allyEncryptionLevelValue")->property("text").toString(), QString("3"));
            for (const auto &metric : {QString("ControlPoints"), QString("Damage")}) {
                auto *redBar = item(root, "red" + metric + "Bar");
                auto *blueBar = item(root, "blue" + metric + "Bar");
                QVERIFY(redBar && blueBar);
                QVERIFY(rect(analysis).contains(rect(redBar)));
                QVERIFY(rect(analysis).contains(rect(blueBar)));
                const double expected = metric == "ControlPoints" ? 1250.0 / 980 : 12345678.0 / 98765432;
                QVERIFY(qAbs(redBar->width() / blueBar->width() - expected) < 0.001);
            }
            QVERIFY(!item(root, "redOutpostHealth"));
            QVERIFY(!item(root, "blueOutpostHealth"));
            for (const auto &name : {"redBaseHealth", "blueBaseHealth"}) {
                auto *facility = item(root, name);
                QVERIFY(facility);
                QVERIFY(header.contains(rect(facility)));
            }
            for (const QString &prefix : {QString("red"), QString("blue")}) {
                auto *logo = item(root, prefix + "TeamLogo");
                auto *value = item(item(root, prefix + "BaseHealth"), "baseHealthValue");
                QVERIFY(logo && value);
                QVERIFY(!item(root, prefix + "BaseStatusText"));
                QCOMPARE(logo->property("status").toInt(), 1);
                QVERIFY(header.contains(rect(logo)));
                QVERIFY(rect(item(root, prefix + "TeamName")).bottom() <= rect(item(root, prefix + "BaseHealth")).top());
                QVERIFY(!rect(logo).intersects(rect(value)));
                QVERIFY(!rect(logo).intersects(rect(item(root, prefix + "TeamName"))));
            }
            QVERIFY(!item(root, "matchConnectionStatus"));
            for (int row = 0; row < 8; ++row) {
                auto *marker = item(root, QString("mapRobot%1").arg(ids[row]));
                auto *track = item(marker, "robotHealthTrack");
                auto *fill = item(marker, "robotHealthFill");
                auto *rank = item(marker, "robotLevelCell");
                auto *level = item(marker, "robotLevelValue");
                auto *unknown = item(marker, "robotHealthUnknown");
                QVERIFY(track && fill && rank && level && unknown);
                auto *body = item(marker, "robotBadgeOutline"); QVERIFY(body);
                const double footprint = std::min(41 * .9, 1.2 * .6 * std::min(layer->width() * 1614 / 1662 / 28,
                                                                            layer->height() * 897 / 946 / 15));
                const auto bodyRect = body->mapRectToItem(layer, QRectF(QPointF(), body->size()));
                QVERIFY(qAbs(bodyRect.width() - footprint) < .001);
                QVERIFY(qAbs(bodyRect.height() - footprint) < .001);
                // 血条、等级和朝向共享车身缩放，不因窗口尺寸改变相互比例。
                QVERIFY(qAbs(rect(track).width() / bodyRect.width() - 42.0 / 41) < .001);
                QCOMPARE(rect(rank).right(), rect(track).left());
                QCOMPARE(rank->height(), track->height());
                QVERIFY(rect(rank).contains(rect(level)));
                QCOMPARE(level->property("text").toString(), QString::number(row + 1));
                QVERIFY(!unknown->isVisible());
                QVERIFY(qAbs(fill->width() / (track->width() - 2) - double(health[row]) / 600) < 0.01);
            }
            QVERIFY(window.grabWindow().save(QString("test-artifacts/hud-%1.png").arg(size.width())));
            const int headerHeight = qCeil(item(root, "matchHeader")->height());
            QVERIFY(window.grabWindow().copy(0, 0, size.width(), headerHeight)
                        .save(QString("test-artifacts/header-%1.png").arg(size.width())));
            const int analysisTop = qRound(rect(analysis).top());
            QVERIFY(window.grabWindow().copy(0, analysisTop, size.width(), size.height() - analysisTop)
                        .save(QString("test-artifacts/analysis-%1x%2.png").arg(size.width()).arg(size.height())));
        }
        // 累计经济降为辅助行后，完整 uint64 读数仍不能截断，尤其检查换行断点两侧。
        logistics.set_remaining_economy(4294967295U);
        logistics.set_total_economy_obtained(18446744073709551615ULL);
        QVERIFY(send(store, logistics, now));
        for (int width : {640, 759, 760, 800, 1024, 1280, 1920}) {
            window.resize(width, 800); QTest::qWait(60);
            auto *analysis = item(root, "battleAnalysisPanel");
            for (const QString &name : {QString("allyEconomyValue"), QString("allyTotalEconomyValue")}) {
                auto *value = item(root, name); QVERIFY(value);
                QCOMPARE(value->property("text").toString(), name == "allyEconomyValue"
                    ? QString("4294967295") : QString("18446744073709551615"));
                QVERIFY2(value->property("contentWidth").toReal() <= value->width() + 1,
                         qPrintable(QString("%1 at %2 px").arg(name).arg(width)));
                QVERIFY(value->property("contentHeight").toReal() <= value->height() + 1);
                QVERIFY(rect(analysis).contains(rect(value)));
                QVERIFY(!rect(value).intersects(rect(item(root, name == "allyEconomyValue"
                    ? "allyEconomyLabel" : "allyTotalEconomyLabel"))));
            }
        }
        // 空态和真实零值在新的辅助行中继续区分。
        logistics.set_remaining_economy(0); logistics.clear_total_economy_obtained();
        QVERIFY(send(store, logistics, now));
        QCOMPARE(item(root, "allyEconomyValue")->property("text").toString(), QString("0"));
        QCOMPARE(item(root, "allyTotalEconomyValue")->property("text").toString(), QString("—"));
        QVERIFY(item(root, "allyEconomyValue")->property("color").value<QColor>()
            != item(root, "allyTotalEconomyValue")->property("color").value<QColor>());
        QVERIFY(!item(root, "mapViewButton"));
        // 顶部只保留基地数字；前哨血量继续由地图显示。
        units.set_base_health(10000); units.set_enemy_base_health(0);
        units.set_outpost_r_health(1000); units.clear_outpost_b_health();
        QVERIFY(send(store, units, now));
        const QStringList facilities{"redBaseHealth", "blueBaseHealth"};
        const QStringList expected{"10000", "0"};
        for (int i = 0; i < facilities.size(); ++i) {
            auto *facility = item(root, facilities[i]);
            auto *value = item(facility, "baseHealthValue");
            QVERIFY(value);
            QTRY_COMPARE(value->property("text").toString(), expected[i]);
            QVERIFY(value->property("contentWidth").toReal() <= value->width() + 1);
            QVERIFY(!value->property("truncated").toBool());
        }
        QCOMPARE(model.match()["blueBaseStatusText"].toString(), QString("基地已摧毁"));
        units.clear_base_status();
        QVERIFY(send(store, units, now));
        QCOMPARE(model.match()["redBaseStatusText"].toString(), QString("状态未知"));
        ly27::wire::RobotStaticStatus missingMaximum;
        missingMaximum.set_robot_id(3); missingMaximum.set_level(2);
        QVERIFY(send(store, missingMaximum, now));
        auto *marker = item(root, "mapRobot3");
        QTRY_COMPARE(item(marker, "robotHealthFill")->width(), 0.0);
        QVERIFY(item(marker, "robotHealthUnknown")->isVisible());
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
            QCOMPARE(model.match()["allyTotalEconomy"].toInt(), -1);
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
            QCOMPARE(model.match()["allyTotalEconomy"].toString(), QString("1000"));
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
            QCOMPARE(model.match()["allyTotalEconomy"].toInt(), -1);
            QCOMPARE(model.match()[allyKey].toUInt(), 42U);
            logistics.set_remaining_economy(245);
            logistics.set_total_economy_obtained(18446744073709551615ULL);
            QVERIFY(send(store, logistics, now));
            QCOMPARE(model.match()["allyEconomy"].toInt(), 245);
            QCOMPARE(model.match()["allyTotalEconomy"].toString(), QString("18446744073709551615"));
            logistics.clear_total_economy_obtained(); QVERIFY(send(store, logistics, now));
            QCOMPARE(model.match()["allyEconomy"].toInt(), 245);
            QCOMPARE(model.match()["allyTotalEconomy"].toInt(), -1);
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

    void radarOnlyFramesDriveEightMapMarkers()
    {
        for (const auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            qint64 now = 1000000000;
            const auto terminal = identity(team);
            ly27::StateStore store(terminal, nullptr, [&now] { return now; });
            store.reset("radar-map"); store.setConnected(true);
            ly27::TerminalViewModel model(store, terminal, "红", "蓝");
            ly27::MapWindow window(&model);
            QCOMPARE(window.status(), QQuickView::Ready);
            window.show();
            ly27::wire::RadarInfoToClient radar;
            for (int i = 0; i < 8; ++i) {
                auto *robot = radar.add_robot_info();
                robot->set_target_pos_x((i + 1) * 100);
                robot->set_target_pos_y((i + 1) * 100);
                robot->set_is_high_light(i % 3);
            }
            QVERIFY(send(store, radar, now));
            auto *root = window.rootObject();
            auto *layer = item(root, "mapMarkerLayer");
            QVERIFY(layer);
            const QList<int> ids = team == ly27::Team::Red
                ? QList<int>{101, 103, 104, 107, 1, 3, 4, 7}
                : QList<int>{1, 3, 4, 7, 101, 103, 104, 107};
            for (int i = 0; i < ids.size(); ++i) {
                auto *marker = item(root, "mapRobot" + QString::number(ids[i]));
                QVERIFY(marker);
                QTRY_VERIFY(marker->isVisible());
                const auto expected = ly27::RobotMapModel::projectPosition(
                    i + 1, i + 1, team == ly27::Team::Blue);
                QTRY_VERIFY(qAbs(marker->x() - expected.x() * layer->width()) < 0.5);
                QTRY_VERIFY(qAbs(marker->y() - expected.y() * layer->height()) < 0.5);
            }
            now += 1100000000LL; model.refresh();
            auto *marker = item(root, "mapRobot3");
            QVERIFY(marker->isVisible());
            QTRY_VERIFY(marker->opacity() < 0.5);
            store.setConnected(false);
            QTRY_VERIFY(!marker->isVisible());
            window.setSource(QUrl());
        }
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
