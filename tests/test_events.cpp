#include "EventListModel.hpp"
#include "rm2027.pb.h"
#include <QtTest>

namespace {
ly27::TerminalIdentity identity(ly27::Team team = ly27::Team::Red)
{
    ly27::TerminalIdentity i; i.team = team; i.role = ly27::Role::Infantry;
    i.robotId = team == ly27::Team::Red ? 3 : 103; i.stationId = "events-test"; return i;
}
bool apply(ly27::StateStore &store, const google::protobuf::Message &message, qint64 now, bool retained = false)
{
    return store.apply(ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
        QByteArray::fromStdString(message.SerializeAsString())), 1700000000000, now, retained);
}
bool sendEvent(ly27::StateStore &store, int id, const QString &p, qint64 now, bool retained = false)
{
    ly27::wire::Event e; e.set_event_id(id); e.set_param(p.toStdString()); return apply(store, e, now, retained);
}
QString value(const ly27::EventListModel &m, int row, int role = ly27::EventListModel::Text)
{
    return m.data(m.index(row, 0), role).toString();
}
}
class EventTest : public QObject {
    Q_OBJECT
private slots:
    void descriptions_data()
    {
        QTest::addColumn<int>("id"); QTest::addColumn<QString>("parameter"); QTest::addColumn<QString>("expected");
        QTest::newRow("kill") << 1 << "1,101" << "蓝方重装击毁了红方重装";
        QTest::newRow("self") << 1 << "3,3" << "红方3号步兵阵亡";
        QTest::newRow("environment") << 1 << "103,10" << "蓝方3号步兵阵亡";
        QTest::newRow("outpost") << 2 << "11" << "红方前哨站被摧毁";
        QTest::newRow("rune-count") << 3 << "10,9.6" << "大能量机关：已激活10个灯臂，平均环数9.6";
        QTest::newRow("rune-active") << 4 << "2" << "大能量机关已激活";
        QTest::newRow("r-outpost") << 5 << "2" << "R前哨阵地被红方完全控制";
        QTest::newRow("b-outpost") << 6 << "3" << "B前哨阵地被蓝方临时控制";
        QTest::newRow("r-base") << 7 << "2" << "R基地阵地被蓝方临时控制";
        QTest::newRow("b-base") << 8 << "1" << "B基地阵地被蓝方完全控制";
        QTest::newRow("dart") << 9 << "2，5" << "蓝方飞镖命中基地末端移动目标";
        QTest::newRow("gate") << 10 << "" << "对方飞镖闸门开启";
        QTest::newRow("base-attack") << 11 << "" << "基地正在遭受攻击！";
        QTest::newRow("outpost-stop") << 12 << "" << "对方前哨站停转";
        QTest::newRow("armor") << 13 << "" << "对方基地护甲展开";
        QTest::newRow("buffer") << 14 << "" << "重装战亡临时激活，进入强制退出缓冲期";
        QTest::newRow("assembly") << 15 << "0" << "装配成功";
        QTest::newRow("assembly-death") << 15 << "4" << "重装战亡，装配失败";
        QTest::newRow("assembly-unplug") << 15 << "1" << "能量单元拔出，装配失败";
        QTest::newRow("assembly-timeout") << 15 << "2" << "装配超时，装配失败";
        QTest::newRow("assembly-leave") << 15 << "3" << "离开装配区过久，装配失败";
        QTest::newRow("assembly-cooperate") << 15 << "5" << "四级难度未满足协作时限，装配失败";
        QTest::newRow("assembly-exit") << 15 << "6" << "主动退出装配";
        QTest::newRow("assembly-empty") << 15 << "7" << "装配完成但未检测到能量单元";
        QTest::newRow("assembly-forced") << 15 << "8" << "缓冲期到期，装配流程强制结束";
        QTest::newRow("module") << 16 << "" << "重装机器人成功安装模块化42mm发射机构";
        QTest::newRow("malformed") << 1 << "1,999999999999" << "事件 #1：1,999999999999";
        QTest::newRow("invalid-control") << 5 << "100" << "事件 #5：100";
        QTest::newRow("unknown") << 99 << "<b>未知</b>" << "事件 #99：<b>未知</b>";
    }
    void descriptions()
    {
        QFETCH(int, id); QFETCH(QString, parameter); QFETCH(QString, expected);
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&] { return now; }); store.reset("test"); store.setConnected(true);
        ly27::EventListModel model(store, ly27::Team::Red);
        QVERIFY(sendEvent(store, id, parameter, now));
        QCOMPARE(model.rowCount(), 1); QCOMPARE(value(model, 0), expected);
    }
    void historyTimeColorAndLifecycle()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&] { return now; }); store.reset("session1"); store.setConnected(true);
        ly27::EventListModel model(store, ly27::Team::Red);
        ly27::wire::GameStatus game; game.set_current_round(1); game.set_stage_countdown_sec(287);
        QVERIFY(apply(store, game, now));
        QVERIFY(sendEvent(store, 1, "1,101", now, true)); QCOMPARE(model.rowCount(), 0);
        QVERIFY(sendEvent(store, 1, "1,101", now)); QVERIFY(sendEvent(store, 1, "1,101", now));
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(value(model, 0, ly27::EventListModel::Time), QString("04:47"));
        QCOMPARE(value(model, 0, ly27::EventListModel::Color), QString("#FF737A"));
        QVERIFY(sendEvent(store, 1, "101,1", now));
        QCOMPARE(value(model, 2, ly27::EventListModel::Color), QString("#60D8B7"));
        now += 5000000000LL; store.refresh(); QCOMPARE(model.rowCount(), 3);
        QVERIFY(sendEvent(store, 11, "", now));
        QCOMPARE(value(model, 3, ly27::EventListModel::TimeSource), QString("接收时间"));
        store.setConnected(false); store.setConnected(true); QCOMPARE(model.rowCount(), 4);
        QVERIFY(sendEvent(store, 11, "", now)); QCOMPARE(model.rowCount(), 5);
        game.set_current_round(2); QVERIFY(apply(store, game, now)); QCOMPARE(model.rowCount(), 0);
        QVERIFY(sendEvent(store, 15, "0", now)); QCOMPARE(model.rowCount(), 1);
        store.reset("session2"); QCOMPARE(model.rowCount(), 0);
    }
    void boundsRestoreAndNoClockDuplication()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(ly27::Team::Blue), nullptr, [&] { return now; }); store.reset("test"); store.setConnected(true);
        ly27::EventListModel model(store, ly27::Team::Blue);
        QVERIFY(sendEvent(store, 1, "1,101", now));
        QCOMPARE(value(model, 0, ly27::EventListModel::Color), QString("#60D8B7"));
        ly27::wire::Event empty; empty.set_param("missing id"); QVERIFY(apply(store, empty, now));
        QCOMPARE(model.rowCount(), 1);
        for (int i = 0; i < 60; ++i) QVERIFY(sendEvent(store, 99, QString::number(i), now));
        QCOMPARE(model.rowCount(), 50); QCOMPARE(value(model, 0), QString("事件 #99：10"));
        QCOMPARE(value(model, 49), QString("事件 #99：59"));
        ly27::EventListModel restored(store, ly27::Team::Blue);
        QCOMPARE(restored.rowCount(), 50); QCOMPARE(value(restored, 0), value(model, 0));
        QCOMPARE(value(restored, 0, ly27::EventListModel::TimeSource), QString("接收时间"));
        QVERIFY(sendEvent(store, 99, QString(5000, 'x'), now));
        QVERIFY(value(model, 49).size() < 300);
        now += 10000000000LL; store.refresh(); QCOMPARE(model.rowCount(), 50);
    }
    void incomeUsesTotalCounterAndIgnoresSpendingMissingRetainedAndReconnect()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&] { return now; }); store.reset("income"); store.setConnected(true);
        ly27::EventListModel model(store, ly27::Team::Red);
        ly27::wire::GameStatus g; g.set_current_round(1); g.set_current_stage(4); g.set_stage_countdown_sec(419);
        QVERIFY(apply(store, g, now));
        ly27::wire::GlobalLogisticsStatus l; l.set_total_economy_obtained(0); l.set_remaining_economy(0);
        QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 0);
        l.set_total_economy_obtained(800); l.set_remaining_economy(800); now += 100000000;
        QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 1);
        QCOMPARE(value(model, 0), QString("获得800金币"));
        QCOMPARE(value(model, 0, ly27::EventListModel::Color), QString("#FFFFFF"));
        QCOMPARE(value(model, 0, ly27::EventListModel::Time), QString("06:59"));
        QCOMPARE(model.data(model.index(0, 0), ly27::EventListModel::EventId).toInt(), 0);
        QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 1);
        l.set_remaining_economy(100); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 1);
        // 收入与消费同时发生导致余额降低时，累计收入仍能反映真实增量。
        l.set_total_economy_obtained(900); l.set_remaining_economy(20); QVERIFY(apply(store, l, now));
        QCOMPARE(model.rowCount(), 2); QCOMPARE(value(model, 1), QString("获得100金币"));
        l.set_total_economy_obtained(5); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 2);
        l.clear_total_economy_obtained(); QVERIFY(apply(store, l, now));
        l.set_total_economy_obtained(1000); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 2);
        l.set_total_economy_obtained(1500); QVERIFY(apply(store, l, now, true)); QCOMPARE(model.rowCount(), 2);
        l.set_total_economy_obtained(2000); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 2);
        store.setConnected(false); store.setConnected(true);
        QVERIFY(apply(store, g, now)); l.set_total_economy_obtained(3000); QVERIFY(apply(store, l, now));
        QCOMPARE(model.rowCount(), 2);
        l.set_total_economy_obtained(3100); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 3);
        now += 4000000000LL; QVERIFY(apply(store, g, now));
        l.set_total_economy_obtained(9000); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 3);
        // 界面后创建时只建立当前收入基线，不把历史总额作为新收入。
        ly27::EventListModel late(store, ly27::Team::Red); QCOMPARE(late.rowCount(), 0);
        l.set_total_economy_obtained(9100); QVERIFY(apply(store, l, now));
        QCOMPARE(late.rowCount(), 1); QCOMPARE(value(late, 0), QString("获得100金币"));
        g.set_current_round(2); QVERIFY(apply(store, g, now)); QCOMPARE(model.rowCount(), 0);
        l.set_total_economy_obtained(0); QVERIFY(apply(store, l, now));
        l.set_total_economy_obtained(800); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 1);
        g.set_current_stage(5); QVERIFY(apply(store, g, now));
        l.set_total_economy_obtained(900); QVERIFY(apply(store, l, now)); QCOMPARE(model.rowCount(), 1);
        store.reset("new-income"); QCOMPARE(model.rowCount(), 0);
    }
    void incomeDeltaKeepsUint64Precision()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&] { return now; }); store.reset("large-income"); store.setConnected(true);
        ly27::EventListModel model(store, ly27::Team::Red);
        ly27::wire::GameStatus g; g.set_current_stage(4); QVERIFY(apply(store, g, now));
        ly27::wire::GlobalLogisticsStatus l;
        l.set_total_economy_obtained(9007199254740993ULL); QVERIFY(apply(store, l, now));
        l.set_total_economy_obtained(9007199254740994ULL); QVERIFY(apply(store, l, now));
        QCOMPARE(model.rowCount(), 1); QCOMPARE(value(model, 0), QString("获得1金币"));
    }
};
QTEST_GUILESS_MAIN(EventTest)
#include "test_events.moc"
