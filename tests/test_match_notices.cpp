#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "rm2027.pb.h"

#include <QDir>
#include <QGuiApplication>
#include <QQuickItem>
#include <QPointer>
#include <QQmlProperty>
#include <QSGRendererInterface>
#include <QtTest>

namespace {
ly27::TerminalIdentity identity(bool commander = false, bool blue = false)
{
    ly27::TerminalIdentity i; i.team = blue ? ly27::Team::Blue : ly27::Team::Red;
    i.role = commander ? ly27::Role::Commander : ly27::Role::Infantry;
    if (!commander) i.robotId = blue ? 103 : 3;
    i.stationId = "notice-test"; return i;
}
struct Scene {
    qint64 now = 1000000000;
    ly27::StateStore store;
    ly27::TerminalViewModel model;
    explicit Scene(bool commander = false, bool blue = false)
        : store(identity(commander, blue), nullptr, [this] { return now; }),
          model(store, identity(commander, blue), "红方", "蓝方")
    { store.reset("notices"); store.setConnected(true); }
    bool send(const google::protobuf::Message &message, bool retained = false)
    {
        ++now;
        return store.apply(ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
            QByteArray::fromStdString(message.SerializeAsString())), 1700000000000, now, retained);
    }
    ly27::wire::GameStatus clock(int elapsed, int round = 1, bool paused = false)
    {
        ly27::wire::GameStatus g; g.set_current_stage(4); g.set_current_round(round);
        g.set_stage_elapsed_sec(elapsed); g.set_stage_countdown_sec(420 - elapsed); g.set_is_paused(paused); return g;
    }
    bool event(int id, const char *param = nullptr, bool retained = false)
    {
        ly27::wire::Event e; e.set_event_id(id); if (param) e.set_param(param); return send(e, retained);
    }
};
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
bool settleNotices(QQuickItem *root)
{
    QCoreApplication::processEvents();
    return QTest::qWaitFor([root] {
        auto *strip = item(root, "mechanismNotices");
        return strip && !strip->property("motionRunning").toBool();
    }, 2000);
}

}
class MatchNoticesTest : public QObject {
    Q_OBJECT
private slots:
    void scheduledNodesAndNoHistoryReplay()
    {
        for (bool commander : {false, true}) {
            Scene s(commander);
            QVERIFY(s.send(s.clock(0))); QCOMPARE(s.model.notices()->timed().size(), 1);
            QVERIFY(s.model.notices()->timed().first().toMap()["text"].toString().contains("小能量"));
            for (int elapsed : {30, 90, 180, 240, 255, 330}) {
                QVERIFY(s.send(s.clock(elapsed - 5)));
                const auto preview = s.model.notices()->timed(); QCOMPARE(preview.size(), 1);
                QCOMPARE(preview.first().toMap()["seconds"].toInt(), 5);
                QVERIFY(preview.first().toMap()["text"].toString().contains("新增 1 次"));
                QVERIFY(s.send(s.clock(elapsed - 1))); QVERIFY(s.send(s.clock(elapsed)));
                QCOMPARE(s.model.notices()->timed().size(), 1);
                QCOMPARE(s.model.notices()->timed().first().toMap()["seconds"].toInt(), 0);
                QVERIFY(s.send(s.clock(elapsed + 4))); QVERIFY(s.model.notices()->timed().isEmpty());
            }
            QVERIFY(s.send(s.clock(415))); QVERIFY(s.model.notices()->timed().first().toMap()["text"].toString().contains("剩余 5 秒"));
            // 同一局回拨不会再次宣告已发放机会。
            QVERIFY(s.send(s.clock(89))); QVERIFY(s.model.notices()->timed().isEmpty());
            QVERIFY(s.send(s.clock(90))); QVERIFY(s.model.notices()->timed().isEmpty());
            // 新局解除去重，中途接入只预告未来，不补播刚过去的节点。
            QVERIFY(s.send(s.clock(90, 2))); QVERIFY(s.model.notices()->timed().isEmpty());
            QVERIFY(s.send(s.clock(175, 2))); QVERIFY(!s.model.notices()->timed().isEmpty());
            QVERIFY(s.send(s.clock(176, 2, true))); QVERIFY(s.model.notices()->timed().isEmpty());
            QVERIFY(s.send(s.clock(177, 2))); QVERIFY(!s.model.notices()->timed().isEmpty());
            s.store.setConnected(false); QVERIFY(s.model.notices()->timed().isEmpty());
            s.store.setConnected(true); QVERIFY(s.send(s.clock(180, 2))); QVERIFY(s.model.notices()->timed().isEmpty());
            QVERIFY(s.send(s.clock(239, 2))); s.now += 4000000000LL;
            QVERIFY(s.send(s.clock(240, 2))); QVERIFY(s.model.notices()->timed().isEmpty());
            auto incomplete = s.clock(254, 2); incomplete.clear_stage_elapsed_sec();
            QVERIFY(s.send(incomplete)); QVERIFY(s.model.notices()->timed().isEmpty());
            auto shortened = s.clock(254, 2); shortened.set_stage_countdown_sec(90);
            QVERIFY(s.send(shortened)); QVERIFY(s.model.notices()->timed().isEmpty());
            QVERIFY(s.send(s.clock(255, 2), true)); QVERIFY(s.model.notices()->timed().isEmpty());
        }
    }
    void currentProtocolEventsAndLifecycle()
    {
        for (bool commander : {false, true}) for (bool blue : {false, true}) {
            Scene s(commander, blue); QVERIFY(s.send(s.clock(40)));
            const QList<QPair<int, const char *>> events{{3,"10,9.6"}, {4,"2"}, {5,"2"}, {6,"4"}, {7,"1"}, {8,"2"},
                {9,"1,5"}, {10,nullptr}, {11,nullptr}, {12,nullptr}, {13,nullptr}, {14,nullptr}, {15,"0"}, {16,nullptr}};
            for (const auto &event : events) {
                QVERIFY(s.event(event.first, event.second));
                const auto current = s.model.notices()->event(); QCOMPARE(current["id"].toInt(), event.first);
                QVERIFY(!current["text"].toString().startsWith("事件 #"));
                QVERIFY(s.event(event.first, event.second)); QCOMPARE(s.model.notices()->event(), current);
            }
            QVERIFY(s.event(14)); QVERIFY(s.model.notices()->event()["text"].toString().contains("重装战亡"));
            QVERIFY(!s.model.notices()->event()["text"].toString().contains("请求四级"));
            const auto before = s.model.notices()->event();
            QVERIFY(s.event(15, "99")); QCOMPARE(s.model.notices()->event(), before);
            QVERIFY(s.event(4)); QCOMPARE(s.model.notices()->event(), before);
            QVERIFY(s.event(9, "3,0")); QCOMPARE(s.model.notices()->event(), before);
            QVERIFY(s.event(17)); QCOMPARE(s.model.notices()->event(), before);
            QVERIFY(s.event(10, nullptr, true)); QCOMPARE(s.model.notices()->event(), before);
            // 击杀和摧毁沿用原有图形播报，不重复占用本提示卡。
            QVERIFY(s.event(1, "103,3")); QCOMPARE(s.model.notices()->event(), before);
            QVERIFY(s.event(2, "111")); QCOMPARE(s.model.notices()->event(), before);
            QVERIFY(s.send(s.clock(41, 1, true))); QVERIFY(s.model.notices()->event().isEmpty());
            QVERIFY(s.event(11)); QVERIFY(s.model.notices()->event().isEmpty());
            QVERIFY(s.send(s.clock(42))); QVERIFY(s.event(10));
            s.store.setConnected(false); QVERIFY(s.model.notices()->event().isEmpty());
            s.store.setConnected(true); QVERIFY(s.send(s.clock(43))); QVERIFY(s.model.notices()->event().isEmpty());
            QVERIFY(s.event(11)); QVERIFY(s.send(s.clock(0, 2))); QVERIFY(s.model.notices()->event().isEmpty());
            QVERIFY(s.event(11)); s.now += 3100000000LL; s.store.refresh();
            s.model.refresh(); QVERIFY(s.model.notices()->event().isEmpty());
        }
    }
    void actualViewsAndExpiry()
    {
        Scene s; QVERIFY(s.send(s.clock(25))); QVERIFY(s.event(10));
        ly27::wire::Buff b; b.set_robot_id(3); b.set_buff_type(2); b.set_buff_level(25); b.set_buff_left_time(20);
        QVERIFY(s.send(b));
        ly27::MapWindow window(&s.model); window.show(); QCOMPARE(window.status(), QQuickView::Ready);
        auto *root = window.rootObject();
        for (auto size : {QSize(1280,800), QSize(640,480)}) {
            window.resize(size); QTest::qWait(30); QVERIFY(settleNotices(root));
            auto *timed = item(root,"timedMatchNotice"); auto *event = item(root,"liveMatchNotice");
            auto *buffs = item(root,"selfBuffPanel"); auto *map = item(root,"mapArea");
            QVERIFY(timed && timed->isVisible()); QVERIFY(event && event->isVisible()); QVERIFY(buffs && buffs->isVisible());
            const QRectF area(QPointF(), map->size());
            const auto timedRect = timed->mapRectToItem(map, QRectF(QPointF(),timed->size()));
            QVERIFY2(area.contains(timedRect), qPrintable(QString("map=%1x%2 timed=(%3,%4,%5,%6) buffWidth=%7")
                .arg(map->width()).arg(map->height()).arg(timedRect.x()).arg(timedRect.y())
                .arg(timedRect.width()).arg(timedRect.height()).arg(buffs->width())));
            QVERIFY(area.contains(event->mapRectToItem(map, QRectF(QPointF(),event->size()))));
            const auto buffRect = buffs->mapRectToItem(map, QRectF(QPointF(),buffs->size()));
            QVERIFY(area.contains(buffRect));
            QVERIFY(!buffRect.intersects(event->mapRectToItem(map, QRectF(QPointF(),event->size()))));
            QVERIFY(!buffRect.intersects(timed->mapRectToItem(map, QRectF(QPointF(),timed->size()))));
            QDir().mkpath("test-artifacts");
            QVERIFY(window.grabWindow().save(QString("test-artifacts/match-notices-%1.png").arg(size.width())));
        }
        QTRY_VERIFY_WITH_TIMEOUT(s.model.notices()->event().isEmpty(), 5500);
        QVERIFY(s.send(s.clock(100))); QVERIFY(s.event(11));
        QTRY_VERIFY_WITH_TIMEOUT(item(root, "liveMatchNotice")->opacity() > .9, 1500);
        QVERIFY(!item(root, "selfBuffPanel")->mapRectToItem(root, QRectF(QPointF(), item(root, "selfBuffPanel")->size()))
            .intersects(item(root, "liveMatchNotice")->mapRectToItem(root, QRectF(QPointF(), item(root, "liveMatchNotice")->size()))));
        QVERIFY(item(root, "liveMatchNotice")->isVisible());
        window.setSource(QUrl());
    }
    void cardsEnterLeaveAndSlideWithoutRestartingOnTicks()
    {
        Scene s(true); QVERIFY(s.send(s.clock(100)));
        ly27::MapWindow window(&s.model); window.show();
        QCOMPARE(window.status(), QQuickView::Ready);
        auto *root = window.rootObject();
        ly27::wire::GlobalSpecialMechanism mechanisms;
        mechanisms.add_mechanism_id(3); mechanisms.add_mechanism_time_sec(5);
        mechanisms.add_mechanism_id(4); mechanisms.add_mechanism_time_sec(-7);
        QVERIFY(s.send(mechanisms));
        QPointer<QQuickItem> left = item(root, "mechanismNotice3");
        QPointer<QQuickItem> right = item(root, "mechanismNotice4");
        QVERIFY(left && right);
        QTRY_VERIFY(left->opacity() > 0 && left->opacity() < 1);
        QVERIFY(left->property("lift").toReal() < 0);
        QVERIFY(left->property("blurAmount").toReal() > 0);
        auto *artwork = item(left, "animatedNoticeArtwork"); QVERIFY(artwork);
        QCOMPARE(QQmlProperty::read(artwork, "layer.enabled").toBool(),
                 window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software);
        QVERIFY(settleNotices(root));
        QCOMPARE(left->opacity(), 1.0);
        QCOMPARE(left->property("lift").toReal(), 0.0);
        const qreal oldRight = right->x(); QVERIFY(oldRight > 0);
        mechanisms.set_mechanism_time_sec(0, 4); QVERIFY(s.send(mechanisms));
        QTest::qWait(40);
        QCOMPARE(item(root, "mechanismNotice3"), left.data());
        QCOMPARE(left->opacity(), 1.0);
        QVERIFY(!left->property("motionRunning").toBool());
        QCOMPARE(item(left, "mechanismMessage")->property("text").toString(), QString("己方还有 4 秒"));

        mechanisms.Clear(); mechanisms.add_mechanism_id(4); mechanisms.add_mechanism_time_sec(-6);
        QVERIFY(s.send(mechanisms));
        QTRY_VERIFY(left->property("leaving").toBool());
        QTest::qWait(120);
        QVERIFY(left->opacity() > 0 && left->opacity() < 1);
        QVERIFY(left->property("lift").toReal() < 0);
        QCOMPARE(item(left, "mechanismMessage")->property("text").toString(), QString("己方还有 4 秒"));
        QCOMPARE(right->x(), oldRight); // 左框尚未离场，右框保留原位置。
        QDir().mkpath("test-artifacts");
        QVERIFY(window.grabWindow().save("test-artifacts/notices-leaving.png"));
        QTRY_VERIFY(!left->property("presented").toBool());
        QTRY_VERIFY(right->x() > 0 && right->x() < oldRight);
        QVERIFY(window.grabWindow().save("test-artifacts/notices-sliding.png"));
        QCOMPARE(item(root, "mechanismNotice4"), right.data());
        QVERIFY(settleNotices(root)); QCOMPARE(right->x(), 0.0);

        // 离场中重新收到同一机制，从当前透明度/位移接回，不重置成首帧。
        mechanisms.add_mechanism_id(3); mechanisms.add_mechanism_time_sec(3); QVERIFY(s.send(mechanisms));
        QTRY_VERIFY(left->isVisible() && left->opacity() > 0);
        QVERIFY(settleNotices(root));
        mechanisms.mutable_mechanism_id()->RemoveLast(); mechanisms.mutable_mechanism_time_sec()->RemoveLast();
        QVERIFY(s.send(mechanisms)); QTRY_VERIFY(left->property("leaving").toBool());
        QTest::qWait(100);
        const qreal partialOpacity = left->opacity();
        mechanisms.add_mechanism_id(3); mechanisms.add_mechanism_time_sec(2); QVERIFY(s.send(mechanisms));
        QCoreApplication::processEvents();
        QVERIFY(left->opacity() > 0 && left->opacity() >= partialOpacity - .05);
        QVERIFY(settleNotices(root)); QCOMPARE(left->opacity(), 1.0);
        s.store.setConnected(false);
        QVERIFY(!left->isVisible() && !right->isVisible());
        QVERIFY(!item(root, "mechanismNotices")->isVisible());
        QVERIFY(!left->property("motionRunning").toBool());
        window.setSource(QUrl());
    }
    void unifiedCardsFlowAndKillStaysAboveThem()
    {
        for (bool commander : {false, true}) {
            Scene s(commander); QVERIFY(s.send(s.clock(25))); QVERIFY(s.event(10));
            ly27::MapWindow window(&s.model); window.show();
            QCOMPARE(window.status(), QQuickView::Ready);
            auto *root = window.rootObject();
            auto *announcement = item(root, "battleAnnouncement"); QVERIFY(announcement);
            for (const auto size : {QSize(1920, 1080), QSize(1280, 800), QSize(640, 480)}) {
                window.resize(size); QTest::qWait(30); QVERIFY(settleNotices(root));
                const auto fixedPosition = announcement->position();
                ly27::wire::GlobalSpecialMechanism mechanisms;
                mechanisms.add_mechanism_id(3); mechanisms.add_mechanism_time_sec(4);
                mechanisms.add_mechanism_id(4); mechanisms.add_mechanism_time_sec(-2);
                QVERIFY(s.send(mechanisms)); QTest::qWait(30); QVERIFY(settleNotices(root));
                const QStringList names{"mechanismNotice4", "mechanismNotice3", "timedMatchNotice", "liveMatchNotice"};
                auto *viewport = item(root, "mechanismNotices"); QVERIFY(viewport);
                QList<QRectF> bounds;
                QSizeF cardSize;
                for (const auto &name : names) {
                    auto *card = item(root, name); QVERIFY(card && card->isVisible());
                    if (bounds.isEmpty()) cardSize = card->size();
                    QCOMPARE(card->size(), cardSize);
                    const auto box = card->mapRectToItem(viewport, QRectF(QPointF(), card->size()));
                    for (const auto &previous : bounds) QVERIFY(!previous.intersects(box));
                    if (!bounds.isEmpty()) {
                        const auto previous = bounds.last();
                        QVERIFY(box.top() > previous.top() || (qAbs(box.top() - previous.top()) < .5 && box.left() > previous.left()));
                    }
                    bounds.append(box);
                }
                QCOMPARE(announcement->position(), fixedPosition);
                QVERIFY(s.event(1, "103,3")); QTest::qWait(120);
                QVERIFY(announcement->isVisible());
                QCOMPARE(announcement->position(), fixedPosition);
                QVERIFY(announcement->z() > item(root, "matchNotices")->z());
                auto *map = item(root, "mapArea"); QVERIFY(map);
                const auto killBox = announcement->mapRectToItem(map, QRectF(QPointF(), announcement->size()));
                QVERIFY(QRectF(QPointF(), map->size()).contains(killBox));
                // 击杀覆盖提示框，不再通过纵向避让将整组卡片向下推。
                bool overlapsCard = false;
                for (const auto &name : names) {
                    auto *card = item(root, name);
                    const auto box = card->mapRectToItem(map, QRectF(QPointF(), card->size()));
                    overlapsCard |= killBox.intersects(box);
                }
                QVERIFY(overlapsCard);
                QDir().mkpath("test-artifacts");
                QVERIFY(window.grabWindow().save(QString("test-artifacts/unified-notices-%1-%2.png")
                    .arg(commander ? "commander" : "operator").arg(size.width())));
                const qreal contentHeight = viewport->property("contentHeight").toReal();
                if (contentHeight > viewport->height()) {
                    viewport->setProperty("contentY", contentHeight - viewport->height());
                    QTest::qWait(30);
                    auto *last = item(root, names.last());
                    const auto lastBox = last->mapRectToItem(viewport, QRectF(QPointF(), last->size()));
                    QVERIFY(QRectF(QPointF(), viewport->size()).contains(lastBox));
                    QCOMPARE(announcement->position(), fixedPosition);
                    QVERIFY(window.grabWindow().save(QString("test-artifacts/unified-notices-last-row-%1-%2.png")
                        .arg(commander ? "commander" : "operator").arg(size.width())));
                }
                mechanisms.Clear(); QVERIFY(s.send(mechanisms)); QTest::qWait(30); QVERIFY(settleNotices(root));
            }
            window.setSource(QUrl());
        }
    }
};
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv); MatchNoticesTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_match_notices.moc"
