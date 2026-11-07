#include "TerritoryOverlayItem.hpp"
#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPathStroker>
#include <QQuickItem>
#include <QtTest>

namespace {
QImage render(ly27::TerritoryOverlayItem &overlay)
{
    QImage image(overlay.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image); overlay.paint(&painter);
    return image;
}
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
ly27::TerminalIdentity identity(ly27::Team team)
{
    ly27::TerminalIdentity i; i.team = team; i.stationId = "territories";
    i.robotId = team == ly27::Team::Red ? 3 : 103; return i;
}
bool send(ly27::StateStore &store, const google::protobuf::Message &message, qint64 &now, bool retained = false)
{
    ++now;
    return store.apply(ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
        QByteArray::fromStdString(message.SerializeAsString())), 1700000000000, now, retained);
}
}

class TerritoriesTest : public QObject {
    Q_OBJECT
private slots:
    void transitionsAreContinuousAndDoNotReplaySnapshots()
    {
        QQuickWindow window; window.resize(1662, 946);
        ly27::TerritoryOverlayItem overlay(window.contentItem()); overlay.setSize({1662, 946});
        window.show(); QTest::qWait(40); overlay.setPhase(.17);
        const auto state = [](const char *owner, bool temporary = false) {
            return QVariantMap{{"known", true}, {"controller", owner}, {"temporary", temporary}};
        };
        QVariantMap values{{"rOutpost", state("blue")}, {"bOutpost", state("red")},
                           {"redBase", state("blue")}, {"blueBase", state("blue")}};
        overlay.setTerritories(values);
        const auto animations = overlay.findChildren<QVariantAnimation *>();
        QCOMPARE(animations.size(), 4); // 基地与堡垒不能各自启动一条动画。
        for (auto *a : animations) QCOMPARE(a->state(), QAbstractAnimation::Stopped);
        auto *front = overlay.findChild<QVariantAnimation *>("territoryTransition_rOutpost"); QVERIFY(front);
        auto *base = overlay.findChild<QVariantAnimation *>("territoryTransition_redBase"); QVERIFY(base);
        const auto before = render(overlay);
        values["rOutpost"] = state("red", true); values["redBase"] = state("red", true);
        overlay.setTerritories(values);
        QCOMPARE(front->state(), QAbstractAnimation::Running);
        QCOMPARE(base->state(), QAbstractAnimation::Running);
        QCOMPARE(render(overlay), before); // 转换首帧保持原样，不瞬间切色。
        front->pause(); base->pause(); front->setCurrentTime(300); base->setCurrentTime(300);
        const auto middle = render(overlay); QVERIFY(middle != before);
        auto metadata = values["rOutpost"].toMap(); metadata["advancing"] = "red"; metadata["seconds"] = 4;
        values["rOutpost"] = metadata; overlay.setTerritories(values);
        QCOMPARE(front->currentTime(), 300); QCOMPARE(front->state(), QAbstractAnimation::Paused);
        metadata["seconds"] = 3; values["rOutpost"] = metadata; overlay.setTerritories(values);
        QCOMPARE(front->currentTime(), 300);
        overlay.setBlueView(true); QCOMPARE(front->currentTime(), 300); overlay.setBlueView(false);
        values["rOutpost"] = state("red", true); overlay.setTerritories(values);
        QCOMPARE(render(overlay), middle);

        // 过渡中再次反转，从正在显示的中间帧接续，不跳回旧起点。
        values["rOutpost"] = state("blue"); overlay.setTerritories(values);
        QCOMPARE(render(overlay), middle); QCOMPARE(front->state(), QAbstractAnimation::Running);
        front->pause(); front->setCurrentTime(600); base->setCurrentTime(600);
        ly27::TerritoryOverlayItem reference; reference.setAnimating(false);
        reference.setSize(overlay.size()); reference.setPhase(overlay.phase()); reference.setTerritories(values);
        QCOMPARE(render(overlay), render(reference));

        // 同色临时转完全也必须平滑关闭间隙。
        values["redBase"] = state("red"); overlay.setTerritories(values);
        QCOMPARE(base->state(), QAbstractAnimation::Running);
        values["redBase"] = QVariantMap{{"known", false}}; overlay.setTerritories(values);
        QCOMPARE(base->state(), QAbstractAnimation::Stopped);
        reference.setTerritories(values); QCOMPARE(render(overlay), render(reference));
        values["redBase"] = state("blue"); overlay.setTerritories(values);
        QCOMPARE(base->state(), QAbstractAnimation::Stopped); // 恢复有效数据直接建立当前快照。
        values["redBase"] = state("red", true); overlay.setTerritories(values);
        QCOMPARE(base->state(), QAbstractAnimation::Running);
        window.hide(); QCOMPARE(base->state(), QAbstractAnimation::Stopped);
        reference.setTerritories(values); QCOMPARE(render(overlay), render(reference));
        window.show(); QCOMPARE(base->state(), QAbstractAnimation::Stopped);
        values["redBase"] = state("red"); overlay.setTerritories(values);
        // 等待实际完成信号状态，避免繁忙机器首帧调度稍晚造成固定等待时间误报。
        QTRY_COMPARE_WITH_TIMEOUT(base->state(), QAbstractAnimation::Stopped, 1500);
        overlay.setAnimating(false); reference.setPhase(overlay.phase()); reference.setTerritories(values);
        QCOMPARE(render(overlay), render(reference));
    }
    void ownershipIsAbsoluteAndBaseShapesShareState()
    {
        for (auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            qint64 now = 1000000000;
            ly27::StateStore store(identity(team), nullptr, [&] { return now; });
            store.reset("ownership"); store.setConnected(true);
            ly27::TerminalViewModel model(store, identity(team), "红", "蓝");
            ly27::TerritoryOverlayItem overlay; overlay.setAnimating(false);
            ly27::wire::GlobalUnitStatus u;
            const bool allyRed = team == ly27::Team::Red;
            for (int r = 1; r <= 4; ++r) for (int b = 1; b <= 4; ++b)
                for (int own = 1; own <= 2; ++own) for (int enemy = 1; enemy <= 2; ++enemy) {
                    u.set_outpost_r_status(r); u.set_outpost_b_status(b);
                    u.set_base_shield(own); u.set_enemy_base_shield(enemy);
                    QVERIFY(send(store, u, now));
                    const auto values = model.territories(); overlay.setTerritories(values);
                    struct Expected { const char *key; bool red; bool temporary; };
                    const int redBase = allyRed ? own : enemy, blueBase = allyRed ? enemy : own;
                    for (const auto &entry : {Expected{"rOutpost", r <= 2 ? allyRed : !allyRed, r == 2 || r == 3},
                        Expected{"bOutpost", b <= 2 ? allyRed : !allyRed, b == 2 || b == 3},
                        Expected{"redBase", redBase == 1, redBase == 2}, Expected{"blueBase", blueBase == 2, blueBase == 2}}) {
                        const auto actual = values.value(entry.key).toMap();
                        QVERIFY(actual["known"].toBool());
                        QCOMPARE(actual["controller"].toString(), QString(entry.red ? "red" : "blue"));
                        QCOMPARE(actual["temporary"].toBool(), entry.temporary);
                        QCOMPARE(actual["color"].toString(), QString(entry.red ? "#FF737A" : "#62C9FF"));
                    }
                    QCOMPARE(overlay.regionState(2), overlay.regionState(3));
                    QCOMPARE(overlay.regionState(4), overlay.regionState(5));
                    model.setBlueView(!model.blueView()); QCOMPARE(model.territories(), values);
                }
            QSignalSpy changes(&model, &ly27::TerminalViewModel::territoriesChanged);
            QVERIFY(send(store, u, now)); u.set_total_cp_ally(200); QVERIFY(send(store, u, now));
            model.refresh(); QCOMPARE(changes.size(), 0);
            u.set_outpost_r_status(0); u.set_outpost_b_status(5); u.set_base_shield(3); u.set_enemy_base_shield(0);
            QVERIFY(send(store, u, now));
            for (const auto &value : model.territories()) QVERIFY(!value.toMap()["known"].toBool());
        }
    }
    void activationAndFreshnessDoNotInventOwnership()
    {
        for (auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            qint64 now = 1000000000;
            ly27::StateStore store(identity(team), nullptr, [&] { return now; });
            store.reset("activation"); store.setConnected(true);
            ly27::TerminalViewModel model(store, identity(team), "红", "蓝");
            ly27::wire::GameStatus g; g.set_current_round(1); g.set_current_stage(4); g.set_is_paused(false);
            QVERIFY(send(store, g, now));
            ly27::wire::GlobalUnitStatus u; u.set_base_shield(1); u.set_enemy_base_shield(1);
            u.set_outpost_r_status(1); u.set_outpost_b_status(4); QVERIFY(send(store, u, now));
            const auto owners = model.territories();
            const QStringList keys{"rOutpost", "bOutpost", "redBase", "blueBase"};
            ly27::wire::GlobalSpecialMechanism m;
            for (int sign : {1, -1}) {
                m.Clear();
                for (int id = 3; id <= 6; ++id) { m.add_mechanism_id(id); m.add_mechanism_time_sec(sign * id); }
                QVERIFY(send(store, m, now));
                const bool actorRed = sign > 0 ? team == ly27::Team::Red : team != ly27::Team::Red;
                for (int index = 0; index < keys.size(); ++index) {
                    const auto state = model.territories()[keys[index]].toMap();
                    QCOMPARE(state["controller"], owners[keys[index]].toMap()["controller"]);
                    QCOMPARE(state["temporary"], owners[keys[index]].toMap()["temporary"]);
                    QCOMPARE(state["advancing"].toString(), QString(actorRed ? "red" : "blue"));
                    QCOMPARE(state["seconds"].toInt(), index + 3);
                }
            }
            g.set_is_paused(true); QVERIFY(send(store, g, now));
            QVERIFY(model.territories()["rOutpost"].toMap()["advancing"].toString().isEmpty());
            QVERIFY(model.territories()["rOutpost"].toMap()["known"].toBool());
            g.set_is_paused(false); QVERIFY(send(store, g, now));
            m.Clear(); m.add_mechanism_id(1); m.add_mechanism_time_sec(4); QVERIFY(send(store, m, now));
            for (const auto &value : model.territories()) QVERIFY(value.toMap()["advancing"].toString().isEmpty());
            m.add_mechanism_id(3); m.add_mechanism_time_sec(2); QVERIFY(send(store, m, now, true));
            QVERIFY(model.territories()["rOutpost"].toMap()["advancing"].toString().isEmpty());
            QVERIFY(send(store, m, now));
            now += 3100000000LL; QVERIFY(send(store, g, now));
            for (const auto &value : model.territories()) {
                QVERIFY(!value.toMap()["known"].toBool()); QVERIFY(value.toMap()["advancing"].toString().isEmpty());
            }
            QVERIFY(send(store, u, now)); QVERIFY(send(store, m, now));
            store.setConnected(false);
            for (const auto &value : model.territories()) {
                QVERIFY(!value.toMap()["known"].toBool()); QVERIFY(value.toMap()["advancing"].toString().isEmpty());
            }
            store.setConnected(true); QVERIFY(send(store, g, now));
            QVERIFY(!model.territories()["rOutpost"].toMap()["known"].toBool());
            QVERIFY(send(store, u, now, true)); QVERIFY(!model.territories()["rOutpost"].toMap()["known"].toBool());
            QVERIFY(send(store, u, now)); QVERIFY(send(store, m, now));
            g.set_current_round(2); QVERIFY(send(store, g, now));
            QVERIFY(!model.territories()["rOutpost"].toMap()["known"].toBool());
            QVERIFY(send(store, u, now)); store.reset("new-session");
            QVERIFY(!model.territories()["rOutpost"].toMap()["known"].toBool());
        }
    }
    void liveStylesAndRobotContrastScreenshots()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(ly27::Team::Red), nullptr, [&] { return now; });
        store.reset("styles"); store.setConnected(true);
        ly27::TerminalViewModel model(store, identity(ly27::Team::Red), "红方", "蓝方");
        ly27::MapWindow window(&model); window.show();
        auto *overlay = qobject_cast<ly27::TerritoryOverlayItem *>(item(window.rootObject(), "territoryOverlay")); QVERIFY(overlay);
        overlay->setAnimating(false); overlay->setPhase(.17);
        ly27::wire::GameStatus g; g.set_current_stage(4); g.set_current_round(1); g.set_is_paused(false);
        g.set_stage_countdown_sec(300); QVERIFY(send(store, g, now));
        ly27::wire::GlobalUnitStatus u; u.set_base_health(8000); u.set_enemy_base_health(9000);
        u.set_outpost_r_health(800); u.set_outpost_b_health(700);
        u.set_outpost_r_status(1); u.set_outpost_b_status(4); u.set_base_shield(1); u.set_enemy_base_shield(1);
        for (int i = 0; i < 8; ++i) u.add_robot_health(200);
        QVERIFY(send(store, u, now));
        ly27::wire::RadarInfoToClient radar;
        const QPointF pixels[]{{655, 420}, {1040, 510}, {171, 461}, {1281, 461}, {655, 520}, {1040, 410}, {1514, 464}, {415, 461}};
        for (const auto &point : pixels) {
            auto *r = radar.add_robot_info(); r->set_target_pos_x(qRound((point.x() - 24) / 1614 * 2800));
            r->set_target_pos_y(qRound((1 - (point.y() - 24) / 897) * 1500)); r->set_is_high_light(0);
        }
        QVERIFY(send(store, radar, now));
        const int ids[]{1, 3, 4, 7, 101, 103, 104, 107};
        for (int id : ids) {
            ly27::wire::RobotStaticStatus r; r.set_robot_id(id); r.set_level(2); r.set_max_health(400); QVERIFY(send(store, r, now));
        }
        QDir().mkpath("test-artifacts");
        const auto screenshot = [&](const QString &name) {
            QTest::qWait(100); return window.grabWindow().save("test-artifacts/territory-control-" + name + ".png");
        };
        QCOMPARE(overlay->territories(), model.territories());
        QVERIFY(screenshot("full"));
        const auto full = render(*overlay);
        u.set_outpost_r_status(3); u.set_outpost_b_status(2); u.set_base_shield(2); QVERIFY(send(store, u, now));
        QVERIFY(screenshot("temporary")); QVERIFY(full != render(*overlay));
        ly27::wire::GlobalSpecialMechanism m;
        for (int id = 3; id <= 6; ++id) { m.add_mechanism_id(id); m.add_mechanism_time_sec(id == 4 ? -2 : id); }
        QVERIFY(send(store, m, now)); QVERIFY(screenshot("advancing"));
        window.resize(960, 600); QVERIFY(screenshot("advancing-960"));
        window.resize(1280, 800);
        QCOMPARE(overlay->regionState(2), overlay->regionState(3));
        QCOMPARE(overlay->regionState(4), overlay->regionState(5));
        // 保存实际主窗口中的首帧、中间帧与完成帧，检查颜色与虚实线转换。
        u.set_outpost_r_status(4); u.set_base_shield(1); QVERIFY(send(store, u, now));
        overlay->setAnimating(true);
        u.set_outpost_r_status(2); u.set_base_shield(2); QVERIFY(send(store, u, now));
        const auto animations = overlay->findChildren<QVariantAnimation *>();
        for (auto *a : animations) if (a->state() == QAbstractAnimation::Running) a->pause();
        QVERIFY(screenshot("transition-start"));
        for (auto *a : animations) if (a->state() == QAbstractAnimation::Paused) a->setCurrentTime(300);
        QVERIFY(screenshot("transition-middle"));
        for (auto *a : animations) if (a->state() == QAbstractAnimation::Paused) a->setCurrentTime(600);
        QVERIFY(screenshot("transition-end"));
        overlay->setAnimating(false);
        const auto state = model.territories(); model.setBlueView(true); QCOMPARE(model.territories(), state);
        QVERIFY(screenshot("blue-view"));
        for (int id : ids) {
            auto *badge = item(window.rootObject(), "mapRobot" + QString::number(id)); QVERIFY(badge);
            auto *outline = item(badge, "robotBadgeOutline"); QVERIFY(outline);
            QVERIFY(outline->width() > item(badge, "robotNumberBadge")->width());
        }
        window.setSource(QUrl());
    }
    void geometryFlowAndClipping()
    {
        ly27::TerritoryOverlayItem overlay; overlay.setAnimating(false);
        overlay.setSize({1662, 946}); QCOMPARE(overlay.regionCount(), 6);
        // 前哨阵地取建筑旁的增益带内部点，而非不属于道路带的建筑本体。
        const QPointF centers[]{{707, 690}, {985, 237}, {171, 461}, {415, 461}, {1514, 464}, {1281, 461}};
        QPainterPath allowed;
        for (int i = 0; i < 6; ++i) {
            auto path = overlay.regionPath(i);
            QVERIFY(path.contains(centers[i]));
            QVERIFY(QRectF(0, 0, 1662, 946).contains(path.boundingRect()));
            allowed = allowed.united(path);
            overlay.setBlueView(true);
            const auto mirror = overlay.regionPath(i);
            QVERIFY(mirror.contains(QPointF(1662 - centers[i].x(), 946 - centers[i].y())));
            QCOMPARE(mirror.boundingRect().topLeft(), QPointF(1662 - path.boundingRect().right(), 946 - path.boundingRect().bottom()));
            overlay.setBlueView(false);
        }
        // 核对图 5-23 的道路直段、两端窄带和本体凹口，不假设两侧严格镜像。
        QVERIFY(overlay.regionPath(0).contains(QPointF(655, 461)));
        QVERIFY(overlay.regionPath(0).contains(QPointF(755, 205)));
        QVERIFY(overlay.regionPath(0).contains(QPointF(675, 725)));
        QVERIFY(overlay.regionPath(1).contains(QPointF(1037, 485)));
        QVERIFY(overlay.regionPath(1).contains(QPointF(1025, 188)));
        QVERIFY(overlay.regionPath(1).contains(QPointF(935, 720)));
        QVERIFY(!overlay.regionPath(0).contains(QPointF(667, 685)));
        QVERIFY(!overlay.regionPath(1).contains(QPointF(1028, 237)));
        for (const auto &point : {QPointF(620, 461), QPointF(706, 461), QPointF(990, 461),
                 QPointF(1080, 461), QPointF(850, 461), QPointF(545, 461), QPointF(1145, 461)})
            QVERIFY(!allowed.contains(point)); // 不吞并路外、中央平台和独立地形跨越增益点。
        QVERIFY(!allowed.contains(QPointF(320, 461))); // 基地与堡垒间的道路不合并。
        QPainterPathStroker glow; glow.setWidth(18);
        glow.setJoinStyle(Qt::RoundJoin); glow.setCapStyle(Qt::RoundCap);
        allowed = allowed.united(glow.createStroke(allowed));
        overlay.setPhase(0); const auto first = render(overlay);
        overlay.setPhase(.17); const auto later = render(overlay);
        int changed = 0;
        for (int y = 0; y < first.height(); y += 3) for (int x = 0; x < first.width(); x += 3) {
            if (!allowed.contains(QPointF(x + .5, y + .5))) {
                // 边沿抗锯齿允许至多两个透明度量级的采样尾迹。
                QVERIFY2(qAlpha(first.pixel(x, y)) <= 2 && qAlpha(later.pixel(x, y)) <= 2,
                    qPrintable(QString("范围外像素 %1,%2 alpha=%3/%4").arg(x).arg(y)
                        .arg(qAlpha(first.pixel(x, y))).arg(qAlpha(later.pixel(x, y)))));
            }
            if (first.pixel(x, y) != later.pixel(x, y)) ++changed;
        }
        QVERIFY(changed > 500);
        overlay.setPhase(1.17); QVERIFY(qAbs(overlay.phase() - .17) < .00001);
        overlay.setSize({0, 0}); QVERIFY(overlay.regionPath(0).isEmpty());
    }
    void mapAlignmentAnimationAndScreenshots()
    {
        ly27::TerminalIdentity identity; identity.stationId = "territory-test"; identity.robotId = 3;
        ly27::StateStore store(identity); store.reset("territories");
        ly27::TerminalViewModel model(store, identity, "红方", "蓝方");
        ly27::MapWindow window(&model); window.show();
        auto *root = window.rootObject();
        auto *overlay = qobject_cast<ly27::TerritoryOverlayItem *>(item(root, "territoryOverlay")); QVERIFY(overlay);
        const auto start = overlay->phase(); QTest::qWait(100); QVERIFY(overlay->phase() != start);
        overlay->setAnimating(false); const auto paused = overlay->phase();
        QTest::qWait(80); QCOMPARE(overlay->phase(), paused);
        overlay->setPhase(.17);
        QDir().mkpath("test-artifacts");
        for (bool blue : {false, true}) {
            model.setBlueView(blue); QCOMPARE(overlay->blueView(), blue);
            for (auto size : {QSize(1920, 1080), QSize(1280, 800), QSize(640, 480)}) {
                window.resize(size); QTest::qWait(80);
                auto *layer = item(root, "mapMarkerLayer"); QVERIFY(layer);
                QCOMPARE(overlay->size(), layer->size()); QCOMPARE(overlay->position(), QPointF());
                QVERIFY(window.grabWindow().save(QString("test-artifacts/territories-%1-%2.png").arg(blue ? "blue" : "red").arg(size.width())));
            }
        }
        overlay->setAnimating(true); window.hide();
        const auto hidden = overlay->phase(); QTest::qWait(80); QCOMPARE(overlay->phase(), hidden);
        window.show(); QTest::qWait(100); QVERIFY(overlay->phase() != hidden);
        window.setSource(QUrl());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    TerritoriesTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_territories.moc"
