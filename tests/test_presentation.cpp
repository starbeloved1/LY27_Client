#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QGuiApplication>
#include <QPointer>
#include <QQuickItem>
#include <QtTest>

namespace {
ly27::TerminalIdentity identity(ly27::Team team)
{
    ly27::TerminalIdentity i; i.team = team; i.role = ly27::Role::Infantry;
    i.robotId = team == ly27::Team::Red ? 3 : 103; i.stationId = "presentation"; return i;
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
    explicit Scene(ly27::Team team = ly27::Team::Red)
        : store(identity(team), nullptr, [this] { return now; }),
          model(store, identity(team), "红方测试队", "蓝方测试队"), window(&model)
    { store.reset("presentation"); store.setConnected(true); window.show(); }
    ~Scene() { window.setSource(QUrl()); }
    bool send(const google::protobuf::Message &message, bool retained = false)
    {
        ++now;
        return store.apply(ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
            QByteArray::fromStdString(message.SerializeAsString())), 1700000000000 + now / 1000000, now, retained);
    }
    QQuickItem *get(const QString &name) { return item(window.rootObject(), name); }
    QString kind() const { return model.phase().value("kind").toString(); }
    bool screenshot(const QString &name)
    {
        QDir().mkpath("test-artifacts");
        QTest::qWait(80);
        return window.grabWindow().save("test-artifacts/" + name + ".png");
    }
};
ly27::wire::GameStatus game(int stage)
{
    ly27::wire::GameStatus g;
    g.set_current_round(1); g.set_total_rounds(3); g.set_red_score(1); g.set_blue_score(0);
    g.set_current_stage(stage); g.set_stage_countdown_sec(stage == 4 ? 419 : 5);
    g.set_is_paused(false); g.set_game_result(255); g.set_end_reason(255); return g;
}
}
class PresentationTest : public QObject {
    Q_OBJECT
private slots:
    void territoryRatesAreInstantAndRelative()
    {
        for (const auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            Scene s(team); auto g = game(4); QVERIFY(s.send(g));
            ly27::wire::GlobalUnitStatus u;
            // 首包即计算；穷举临时/完全占领和两座基地的所有合法组合。
            for (int r = 1; r <= 4; ++r) for (int b = 1; b <= 4; ++b)
                for (int own = 1; own <= 2; ++own) for (int foe = 1; foe <= 2; ++foe) {
                    u.set_outpost_r_status(r); u.set_outpost_b_status(b);
                    u.set_base_shield(own); u.set_enemy_base_shield(foe);
                    u.set_total_cp_ally(123); u.set_total_cp_enemy(456);
                    u.set_total_damage_ally(1234); u.set_total_damage_enemy(5678);
                    QVERIFY(s.send(u));
                    const int ally = (r <= 2 && b <= 2 ? 1 : 0) + (own == 1 && foe == 2 ? 3 : 0);
                    const int enemy = (r >= 3 && b >= 3 ? 1 : 0) + (own == 2 && foe == 1 ? 3 : 0);
                    QVERIFY(s.model.match()["controlNetRate"].isValid());
                    QCOMPARE(s.model.match()["controlNetRate"].toInt(), ally - enemy);
                    const auto before = s.model.match(); s.model.setBlueView(!s.model.blueView());
                    QCOMPARE(s.model.match(), before);
                }
            QVERIFY(!s.model.match().contains("damageNetRate"));
            QVERIFY(!s.get("DamageNetRate")->isVisible());
            QVERIFY(!s.get("redDamageRate"));
            // 红方通信身份时 +4；蓝方身份时也是己方 +4，不随地图旋转变号。
            u.set_outpost_r_status(1); u.set_outpost_b_status(2);
            u.set_base_shield(1); u.set_enemy_base_shield(2); QVERIFY(s.send(u));
            QCOMPARE(s.model.match()["controlNetRate"].toInt(), 4);
            QVERIFY(s.get("ControlPointsNetRate")->property("text").toString().contains("+4 点/s"));
            QCOMPARE(s.get("ControlPointsNetRate")->property("color").value<QColor>(), QColor("#60D8B7"));
            QVERIFY(s.screenshot(team == ly27::Team::Red ? "territory-rate-red" : "territory-rate-blue"));
            u.set_outpost_r_status(3); u.set_outpost_b_status(4);
            u.set_base_shield(2); u.set_enemy_base_shield(1); QVERIFY(s.send(u));
            QCOMPARE(s.model.match()["controlNetRate"].toInt(), -4);
            QCOMPARE(s.get("ControlPointsNetRate")->property("color").value<QColor>(), QColor("#FF737A"));
            // 快进或手工修改累计量不能改变根据阵地算出的瞬时速度。
            g.set_stage_countdown_sec(100); QVERIFY(s.send(g));
            u.set_total_cp_ally(4000000); u.set_total_cp_enemy(0); QVERIFY(s.send(u));
            QCOMPARE(s.model.match()["controlNetRate"].toInt(), -4);
            g.set_is_paused(true); QVERIFY(s.send(g)); QCOMPARE(s.model.match()["controlNetRate"].toInt(), 0);
            g.set_is_paused(false); QVERIFY(s.send(g)); QCOMPARE(s.model.match()["controlNetRate"].toInt(), -4);
            u.clear_outpost_r_status(); QVERIFY(s.send(u)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            u.set_outpost_r_status(0); QVERIFY(s.send(u)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            u.set_outpost_r_status(5); QVERIFY(s.send(u)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            u.set_outpost_r_status(1); u.set_outpost_b_status(3);
            u.set_base_shield(1); u.set_enemy_base_shield(1); QVERIFY(s.send(u));
            QCOMPARE(s.model.match()["controlNetRate"].toInt(), 0);
            QCOMPARE(s.get("ControlPointsNetRate")->property("color").value<QColor>(), QColor("#9baebc"));
            u.set_base_shield(3); QVERIFY(s.send(u)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            u.set_base_shield(1); u.clear_enemy_base_shield(); QVERIFY(s.send(u));
            QVERIFY(!s.model.match()["controlNetRate"].isValid());
            u.set_enemy_base_shield(1); u.clear_outpost_b_status(); QVERIFY(s.send(u));
            QVERIFY(!s.model.match()["controlNetRate"].isValid());
            u.set_outpost_b_status(1); QVERIFY(s.send(u));
            s.now += 3100000000LL; QVERIFY(s.send(g)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            QVERIFY(s.send(u)); QCOMPARE(s.model.match()["controlNetRate"].toInt(), 1);
            QVERIFY(s.send(u, true)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            QVERIFY(s.send(u));
            s.store.setConnected(false); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            s.store.setConnected(true); QVERIFY(s.send(g)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            QVERIFY(s.send(u)); QCOMPARE(s.model.match()["controlNetRate"].toInt(), 1);
            g.set_current_round(2); QVERIFY(s.send(g)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
            QVERIFY(s.send(u)); QCOMPARE(s.model.match()["controlNetRate"].toInt(), 1);
            g.set_current_stage(5); QVERIFY(s.send(g)); QVERIFY(!s.model.match()["controlNetRate"].isValid());
        }
    }
    void killsOutpostsAndAnnouncementLifecycle()
    {
        Scene s; auto g = game(3); QVERIFY(s.send(g));
        g.set_current_stage(4); g.set_stage_countdown_sec(419); QVERIFY(s.send(g));
        s.now += 1300000000LL; QVERIFY(s.send(g));
        auto event = [&](int id, const char *parameter, bool retained = false) {
            ly27::wire::Event e; e.set_event_id(id); e.set_param(parameter); return s.send(e, retained);
        };
        QVERIFY(event(1, "103,3"));
        QCOMPARE(s.model.battleNotice()["text"].toString(), QString("第一滴血"));
        QCOMPARE(s.model.battleNotice()["killer"].toInt(), 3);
        QCOMPARE(s.model.battleNotice()["victim"].toInt(), 103);
        QTest::qWait(150); QVERIFY(s.get("battleAnnouncement")->isVisible());
        QVERIFY(s.screenshot("announcement-first-blood"));
        const auto first = s.model.battleNotice();
        QVERIFY(s.send(g)); s.model.refresh(); QCOMPARE(s.model.battleNotice(), first);
        QVERIFY(event(1, "104,3")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("双杀"));
        QVERIFY(s.model.battleNotice()["token"] != first["token"]);
        QTest::qWait(150); QVERIFY(s.screenshot("announcement-double-kill"));
        QVERIFY(event(1, "107,3")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("三杀"));
        QVERIFY(event(1, "101,3")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("四杀"));
        QVERIFY(event(1, "103,3")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("五杀"));
        s.now += 11000000000LL; QVERIFY(s.send(g));
        QVERIFY(event(1, "104,3")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("摧毁"));
        QVERIFY(event(1, "3,103")); QVERIFY(!s.model.battleNotice()["red"].toBool());
        QTest::qWait(150); QVERIFY(s.screenshot("announcement-blue-kill"));
        QVERIFY(event(1, "4,0")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("阵亡"));
        QVERIFY(event(1, "4,4")); QCOMPARE(s.model.battleNotice()["killer"].toInt(), 0);
        QVERIFY(event(1, "4,10")); QCOMPARE(s.model.battleNotice()["killer"].toInt(), 0);
        QVERIFY(event(1, "104,110")); QCOMPARE(s.model.battleNotice()["killer"].toInt(), 0);
        QVERIFY(event(2, "11")); QCOMPARE(s.model.battleNotice()["kind"].toString(), QString("outpost"));
        QVERIFY(s.model.battleNotice()["red"].toBool());
        QTest::qWait(150); QVERIFY(s.screenshot("announcement-red-outpost"));
        QVERIFY(event(2, "111")); QVERIFY(!s.model.battleNotice()["red"].toBool());
        QTest::qWait(150); QVERIFY(s.screenshot("announcement-blue-outpost"));
        const auto outpost = s.model.battleNotice();
        for (const auto *bad : {"", "0", "100,3", "103,no", "103,3,4", "-1,3"}) QVERIFY(event(1, bad));
        QVERIFY(event(2, "12")); QVERIFY(event(1, "103,3", true));
        QCOMPARE(s.model.battleNotice(), outpost);
        const auto view = s.model.battleNotice(); s.model.setBlueView(true); QCOMPARE(s.model.battleNotice(), view);
        QTRY_VERIFY_WITH_TIMEOUT(s.model.battleNotice().isEmpty(), 4000);
        QVERIFY(!s.get("battleAnnouncement")->isVisible());
        QVERIFY(event(1, "103,3")); s.store.setConnected(false); QVERIFY(s.model.battleNotice().isEmpty());
        s.store.setConnected(true); QVERIFY(s.send(g)); QVERIFY(s.model.battleNotice().isEmpty());
        QVERIFY(event(1, "103,3")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("摧毁"));
        g.set_current_round(2); QVERIFY(s.send(g)); QVERIFY(s.model.battleNotice().isEmpty());
        QVERIFY(event(1, "103,3")); QCOMPARE(s.model.battleNotice()["text"].toString(), QString("摧毁"));
        g.set_current_stage(3); QVERIFY(s.send(g)); QVERIFY(s.model.battleNotice().isEmpty());
        g.set_current_stage(4); QVERIFY(s.send(g)); QVERIFY(event(1, "103,3"));
        QCOMPARE(s.model.battleNotice()["text"].toString(), QString("第一滴血"));
        s.store.reset("new-announcement-session"); QVERIFY(s.model.battleNotice().isEmpty());
        // 晚建窗口只恢复事件列表，不能把旧击杀重新当成即时播报。
        Scene late; QVERIFY(late.send(game(4)));
        ly27::wire::Event e; e.set_event_id(1); e.set_param("103,3"); QVERIFY(late.send(e));
        ly27::TerminalViewModel model(late.store, identity(ly27::Team::Red), "红", "蓝");
        QVERIFY(model.battleNotice().isEmpty());
    }
    void stagesAndOfficialImages()
    {
        Scene s;
        QCOMPARE(s.window.status(), QQuickView::Ready);
        QCOMPARE(s.kind(), QString("none"));
        auto g = game(1); g.set_stage_countdown_sec(180); QVERIFY(s.send(g));
        QCOMPARE(s.kind(), QString("preparation")); QVERIFY(s.screenshot("phase-preparation"));
        g.set_current_stage(2); g.set_stage_countdown_sec(15); QVERIFY(s.send(g));
        QCOMPARE(s.model.phase().value("title").toString(), QString("裁判系统自检"));
        QVERIFY(s.screenshot("phase-self-check"));
        g.set_current_stage(3);
        auto *image = s.get("matchCountdownImage"); QVERIFY(image);
        for (int seconds = 5; seconds >= 0; --seconds) {
            g.set_stage_countdown_sec(seconds); QVERIFY(s.send(g));
            QCOMPARE(s.kind(), QString("countdown"));
            QTRY_COMPARE(image->property("status").toInt(), 1);
            QCOMPARE(image->property("source").toUrl().fileName(), seconds ? QString("freeze_%1.png").arg(seconds) : QString("freeze_start.png"));
            if (seconds == 5) QVERIFY(s.screenshot("phase-countdown"));
        }
        g.set_current_stage(4); g.set_stage_countdown_sec(420); QVERIFY(s.send(g));
        QCOMPARE(s.kind(), QString("start")); QVERIFY(s.screenshot("phase-start"));
        s.now += 1300000000LL; QVERIFY(s.send(g)); QCOMPARE(s.kind(), QString("none"));
        g.set_is_paused(true); QVERIFY(s.send(g)); QCOMPARE(s.kind(), QString("pause"));
        QVERIFY(s.screenshot("phase-pause"));
        g.set_is_paused(false); g.set_game_result(1); QVERIFY(s.send(g));
        // 非结算阶段即便携带胜者，也不能显示胜利画面。
        QCOMPARE(s.kind(), QString("none"));
        g.set_current_stage(3); g.set_stage_countdown_sec(6); QVERIFY(s.send(g)); QCOMPARE(s.kind(), QString("none"));
        g.set_stage_countdown_sec(-1); QVERIFY(s.send(g)); QCOMPARE(s.kind(), QString("none"));
        g.set_stage_countdown_sec(3); QVERIFY(s.send(g)); QCOMPARE(s.kind(), QString("countdown"));
        s.now += 700000000; s.model.refresh(); QCOMPARE(s.kind(), QString("none"));
        QVERIFY(s.send(g, true)); QCOMPARE(s.kind(), QString("none"));
        QVERIFY(s.send(g)); QCOMPARE(s.kind(), QString("countdown"));
        s.store.setConnected(false); QCOMPARE(s.kind(), QString("none"));
        s.store.setConnected(true); QCOMPARE(s.kind(), QString("none"));
    }
    void victoryAnimationRequiresLiveTransition()
    {
        Scene s;
        auto *overlay = s.get("matchPhaseOverlay"); QVERIFY(overlay);
        auto g = game(5); g.set_game_result(1);
        QVERIFY(s.send(g));
        QVERIFY(!s.model.phase()["animateVictory"].toBool());
        QCOMPARE(overlay->property("frame").toInt(), 33);
        QTest::qWait(100); QCOMPARE(overlay->property("frame").toInt(), 33);

        // 重启进入等待结算，随后才收到胜者，也只静态恢复战报。
        s.store.reset("startup-settlement");
        s.store.setConnected(true);
        g.set_game_result(255); QVERIFY(s.send(g));
        g.set_game_result(2); QVERIFY(s.send(g));
        QVERIFY(!s.model.phase()["animateVictory"].toBool());
        QCOMPARE(overlay->property("frame").toInt(), 33);

        // 本次连接见证比赛结束，允许动画；周期上报和刷新不重新从零播放。
        g.set_current_stage(4); QVERIFY(s.send(g));
        g.set_current_stage(5); g.set_game_result(255); QVERIFY(s.send(g));
        g.set_game_result(1); QVERIFY(s.send(g));
        QVERIFY(s.model.phase()["animateVictory"].toBool());
        QCOMPARE(overlay->property("frame").toInt(), 0);
        QTRY_VERIFY(overlay->property("frame").toInt() > 0);
        const int frame = overlay->property("frame").toInt();
        QVERIFY(s.send(g)); s.model.refresh();
        QCOMPARE(overlay->property("frame").toInt(), frame);
        QTRY_COMPARE(overlay->property("frame").toInt(), 33);
        QVERIFY(s.send(g)); QCOMPARE(overlay->property("frame").toInt(), 33);

        g.set_current_stage(4); QVERIFY(s.send(g));
        s.store.setConnected(false); s.store.setConnected(true);
        g.set_current_stage(5); QVERIFY(s.send(g));
        QVERIFY(!s.model.phase()["animateVictory"].toBool());
        QCOMPARE(overlay->property("frame").toInt(), 33);

        // retained、过期、跨局和跨会话的比赛状态均不能提供播放资格。
        g.set_current_stage(4); QVERIFY(s.send(g, true));
        g.set_current_stage(5); QVERIFY(s.send(g));
        QVERIFY(!s.model.phase()["animateVictory"].toBool());
        g.set_current_stage(4); QVERIFY(s.send(g));
        s.now += 700000000; s.model.refresh();
        g.set_current_stage(5); QVERIFY(s.send(g));
        QVERIFY(!s.model.phase()["animateVictory"].toBool());
        g.set_current_stage(4); QVERIFY(s.send(g));
        g.set_current_stage(5); g.set_current_round(2); QVERIFY(s.send(g));
        QVERIFY(!s.model.phase()["animateVictory"].toBool());
        g.set_current_stage(4); QVERIFY(s.send(g));
        s.store.reset("new-victory-session");
        s.store.setConnected(true);
        g.set_current_stage(5); QVERIFY(s.send(g));
        QVERIFY(!s.model.phase()["animateVictory"].toBool());
        QCOMPARE(overlay->property("frame").toInt(), 33);
    }
    void resultsReasonsDismissalAndRoundIsolation()
    {
        Scene s;
        auto g = game(5); QVERIFY(s.send(g)); QCOMPARE(s.kind(), QString("settlement"));
        g.set_game_result(1); g.set_end_reason(2); QVERIFY(s.send(g));
        QCOMPARE(s.kind(), QString("result"));
        QCOMPARE(s.model.phase().value("title").toString(), QString("红方胜利"));
        auto *overlay = s.get("matchPhaseOverlay"); auto *image = s.get("matchVictoryImage");
        QVERIFY(overlay && image);
        QTRY_COMPARE(overlay->property("frame").toInt(), 33);
        QTRY_COMPARE(image->property("status").toInt(), 1);
        QVERIFY(s.screenshot("phase-red-victory"));
        const auto close = s.get("dismissMatchResult"); QVERIFY(close);
        QTest::mouseClick(&s.window, Qt::LeftButton, Qt::NoModifier,
            close->mapToScene(QPointF(close->width() / 2, close->height() / 2)).toPoint());
        QTRY_VERIFY(!overlay->isVisible());
        QVERIFY(s.send(g)); QVERIFY(!overlay->isVisible());
        g.set_current_round(2); QVERIFY(s.send(g)); QVERIFY(overlay->isVisible());
        g.set_game_result(2); g.set_blue_score(1); g.set_end_reason(3); QVERIFY(s.send(g));
        QCOMPARE(s.model.phase().value("art").toString(), QString("blue"));
        QTRY_COMPARE(overlay->property("frame").toInt(), 33);
        QVERIFY(s.screenshot("phase-blue-victory"));
        g.set_current_round(3); g.set_game_result(0); g.set_end_reason(5); QVERIFY(s.send(g));
        QCOMPARE(s.model.phase().value("title").toString(), QString("平局"));
        QCOMPARE(s.model.phase().value("art").toString(), QString());
        QVERIFY(s.screenshot("phase-draw"));
        g.set_game_result(255); g.set_end_reason(6); QVERIFY(s.send(g));
        QCOMPARE(s.model.phase().value("title").toString(), QString("比赛异常终止"));
        QCOMPARE(s.model.phase().value("art").toString(), QString());
        QVERIFY(s.get("matchResultTitle")->isVisible());
        QVERIFY(s.screenshot("phase-terminated"));
        g.set_game_result(1); QVERIFY(s.send(g));
        QCOMPARE(s.model.phase().value("title").toString(), QString("红方胜利"));
        QCOMPARE(s.model.phase().value("description").toString(), QString("异常终止"));
        g.set_game_result(1); g.set_end_reason(7); QVERIFY(s.send(g));
        QCOMPARE(s.model.phase().value("description").toString(), QString("主裁判手动判定胜负"));
        s.store.reset("next-session"); QCOMPARE(s.kind(), QString("none"));
    }
    void mechanismCardsSurviveStatusAndCountdownUpdates()
    {
        Scene s; auto g = game(4); QVERIFY(s.send(g));
        ly27::wire::GlobalSpecialMechanism m;
        m.add_mechanism_id(3); m.add_mechanism_time_sec(5); QVERIFY(s.send(m));
        QPointer<QQuickItem> card = s.get("mechanismNotice3");
        QTRY_VERIFY(card && card->isVisible());
        // 无关状态刷新、重复包和秒数变化都不能销毁面板及其 Canvas。
        ly27::wire::GlobalUnitStatus u;
        for (int n = 0; n < 20; ++n) {
            u.set_total_cp_ally(n); QVERIFY(s.send(u));
            s.model.refresh(); QTest::qWait(5);
            QVERIFY2(card, "无关状态更新销毁了阵地倒计时面板");
            QCOMPARE(s.get("mechanismNotice3"), card.data());
        }
        QVERIFY(s.send(m)); QTest::qWait(5); QVERIFY(card);
        for (int seconds = 4; seconds > 0; --seconds) {
            m.set_mechanism_time_sec(0, seconds); QVERIFY(s.send(m));
            QTest::qWait(20);
            QVERIFY2(card, "倒计时变化销毁了阵地提示面板");
            QCOMPARE(s.get("mechanismNotice3"), card.data());
            QCOMPARE(item(card, "mechanismMessage")->property("text").toString(),
                     QStringLiteral("我方正在占领红方前哨阵地！"));
        }
        // 排序、新增和移除提示只更新现有槽位，不重建控件。
        m.add_mechanism_id(4); m.add_mechanism_time_sec(-2); QVERIFY(s.send(m));
        QPointer<QQuickItem> second = s.get("mechanismNotice4"); QVERIFY(second);
        QTRY_VERIFY(second->isVisible());
        m.set_mechanism_time_sec(0, 5); QVERIFY(s.send(m));
        QTest::qWait(20); QVERIFY(card && second);
        QVERIFY(s.get("mechanismNotice4")->isVisible());
        m.Clear(); QVERIFY(s.send(m));
        QTest::qWait(20); QVERIFY(card && second);
        QTRY_VERIFY(!s.get("mechanismNotices")->isVisible());
        m.add_mechanism_id(3); m.add_mechanism_time_sec(3); QVERIFY(s.send(m));
        QTRY_VERIFY(s.get("mechanismNotice3")->isVisible());
        QVERIFY(card && second);
        QVERIFY(s.screenshot("mechanisms-stable-update"));
    }
    void signedMechanismsClearPauseAndExpiration()
    {
        for (const auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            Scene s(team); auto g = game(4); QVERIFY(s.send(g));
            ly27::wire::GlobalSpecialMechanism m;
            const int times[] = {9, 8, 3, -4, 2, -1};
            for (int i = 0; i < 6; ++i) { m.add_mechanism_id(i + 1); m.add_mechanism_time_sec(times[i]); }
            QVERIFY(s.send(m)); QCOMPARE(s.model.mechanisms().size(), 6);
            QCOMPARE(s.model.mechanisms().first().toMap().value("id").toUInt(), 6U);
            for (const auto &notice : s.model.mechanisms()) {
                const auto n = notice.toMap(); const int id = n.value("id").toInt();
                QCOMPARE(n.value("enemy").toBool(), id == 1 || id == 4 || id == 6);
                QCOMPARE(n.value("seconds").toInt(), qAbs(times[id - 1]));
                auto *card = s.get("mechanismNotice" + QString::number(id));
                QTRY_VERIFY(card && card->isVisible());
            }
            const auto before = s.model.mechanisms(); s.model.setBlueView(!s.model.blueView());
            QCOMPARE(s.model.mechanisms(), before);
            QVERIFY(s.screenshot(team == ly27::Team::Red ? "mechanisms-red" : "mechanisms-blue"));
            g.set_is_paused(true); QVERIFY(s.send(g)); QVERIFY(s.model.mechanisms().isEmpty());
            g.set_is_paused(false); QVERIFY(s.send(g)); QCOMPARE(s.model.mechanisms().size(), 6);
            m.Clear(); QVERIFY(s.send(m)); QVERIFY(s.model.mechanisms().isEmpty());
            m.add_mechanism_id(3); m.add_mechanism_time_sec(-2); QVERIFY(s.send(m));
            QCOMPARE(s.model.mechanisms().first().toMap().value("text").toString(), QString("敌方正在占领红方前哨阵地！\n倒计时2秒"));
            m.set_mechanism_time_sec(0, 2); QVERIFY(s.send(m));
            QCOMPARE(s.model.mechanisms().first().toMap().value("actor").toString(), QString("我方"));
            m.set_mechanism_time_sec(0, 0); QVERIFY(s.send(m)); QVERIFY(s.model.mechanisms().isEmpty());
            m.set_mechanism_time_sec(0, 2); m.add_mechanism_id(4); QVERIFY(s.send(m));
            QVERIFY(s.model.mechanisms().isEmpty());
            m.mutable_mechanism_id()->RemoveLast(); QVERIFY(s.send(m));
            s.now += 3100000000LL; QVERIFY(s.send(g)); QVERIFY(s.model.mechanisms().isEmpty());
            QVERIFY(s.send(m)); QCOMPARE(s.model.mechanisms().size(), 1);
            s.store.setConnected(false); QVERIFY(s.model.mechanisms().isEmpty());
            s.store.setConnected(true); QVERIFY(s.send(g)); QVERIFY(s.model.mechanisms().isEmpty());
            QVERIFY(s.send(m)); QCOMPARE(s.model.mechanisms().size(), 1);
            g.set_current_round(2); QVERIFY(s.send(g)); QVERIFY(s.model.mechanisms().isEmpty());
        }
    }
    void tacticalFieldsRespectTeamAndFreshness()
    {
        for (const auto team : {ly27::Team::Red, ly27::Team::Blue}) {
            Scene s(team); ly27::wire::GlobalUnitStatus u;
            u.set_total_damage_ally(0); u.set_total_damage_enemy(4294967295U); QVERIFY(s.send(u));
            ly27::wire::GlobalLogisticsStatus l; l.set_tech_level(0); l.set_encryption_level(3); QVERIFY(s.send(l));
            const bool red = team == ly27::Team::Red;
            QCOMPARE(s.model.match()[red ? "redDamage" : "blueDamage"].toUInt(), 0U);
            QCOMPARE(s.model.match()[red ? "blueDamage" : "redDamage"].toUInt(), 4294967295U);
            QCOMPARE(s.model.match()["allyTechLevel"].toInt(), 0);
            QCOMPARE(s.model.match()["allyEncryptionLevel"].toInt(), 3);
            s.model.setBlueView(!s.model.blueView());
            QCOMPARE(s.model.match()[red ? "blueDamage" : "redDamage"].toUInt(), 4294967295U);
            s.now += 3100000000LL; s.model.refresh();
            QCOMPARE(s.model.match()["redDamage"].toInt(), -1);
            QCOMPARE(s.model.match()["allyTechLevel"].toInt(), -1);
        }
    }
};
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv); PresentationTest t; return QTest::qExec(&t, argc, argv);
}
#include "test_presentation.moc"
