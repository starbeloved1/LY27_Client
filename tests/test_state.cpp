#include "StateStore.hpp"
#include "rm2027.pb.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QtTest>

namespace {
ly27::TerminalIdentity identity(ly27::Role role = ly27::Role::Infantry,
                                ly27::Team team = ly27::Team::Red)
{
    ly27::TerminalIdentity result;
    result.role = role;
    result.team = team;
    result.stationId = "state-test";
    if (role != ly27::Role::Commander)
        result.robotId = (role == ly27::Role::Heavy ? 1 : 3) + (team == ly27::Team::Blue ? 100 : 0);
    return result;
}

bool send(ly27::StateStore &store, const QString &topic, const google::protobuf::Message &message,
          qint64 now, bool retained = false)
{
    return store.apply(ly27::MessageCodec::decode(topic, QByteArray::fromStdString(message.SerializeAsString())),
                       1700000000000, now, retained);
}
} // 命名空间

class StateTest : public QObject {
    Q_OBJECT
private slots:
    void presenceAndReplacement()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("session-a");
        QCOMPARE(store.topic("GameStatus").validity, QString("unknown"));
        QVERIFY(store.topic("GameStatus").fields.isEmpty());
        QVERIFY(store.topic("GameStatus").toJson()["age_ms"].isNull());
        store.setConnected(true);
        ly27::wire::GameStatus game;
        QVERIFY(send(store, "GameStatus", game, now));
        QCOMPARE(store.topic("GameStatus").reason, QString("missing_fields"));
        game.set_current_stage(0);
        game.set_is_paused(false);
        QVERIFY(send(store, "GameStatus", game, now));
        const auto value = store.topic("GameStatus");
        QVERIFY(value.isFresh());
        QVERIFY(value.fields.contains("current_stage"));
        QCOMPARE(value.fields["current_stage"].toInt(-1), 0);
        QVERIFY(value.fields.contains("is_paused"));
        QCOMPARE(value.fields["is_paused"], QJsonValue(false));
        QVERIFY(!value.fields.contains("game_result"));
        QCOMPARE(value.source["session_id"].toString(), QString("session-a"));
        QCOMPARE(value.source["topic"].toString(), QString("GameStatus"));
        QCOMPARE(value.source["received_utc_ms"].toInteger(), qint64(1700000000000));

        // 不推断官方发送增量；本帧缺失的标量不会从上帧继承。
        game.Clear();
        game.set_stage_countdown_sec(100);
        QVERIFY(send(store, "GameStatus", game, now));
        QVERIFY(!store.topic("GameStatus").fields.contains("current_stage"));
        QCOMPARE(store.topic("GameStatus").fields["stage_countdown_sec"].toInt(), 100);

        ly27::wire::GlobalSpecialMechanism mechanisms;
        mechanisms.add_mechanism_id(2);
        mechanisms.add_mechanism_time_sec(5);
        QVERIFY(send(store, "GlobalSpecialMechanism", mechanisms, now));
        mechanisms.Clear();
        QVERIFY(send(store, "GlobalSpecialMechanism", mechanisms, now));
        QVERIFY(store.topic("GlobalSpecialMechanism").isFresh());
        QVERIFY(store.topic("GlobalSpecialMechanism").fields["mechanism_id"].toArray().isEmpty());
    }

    void freshnessUsesMonotonicTimeAndNotifies()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("timeout"); store.setConnected(true);
        ly27::wire::GameStatus game;
        game.set_current_stage(4);
        QVERIFY(send(store, "GameStatus", game, now));
        QCOMPARE(*store.topic("GameStatus").ttlMs, qint64(600));
        QSignalSpy changed(&store, &ly27::StateStore::changed);
        now += 599000000;
        store.refresh();
        QVERIFY(store.topic("GameStatus").isFresh());
        QCOMPARE(changed.count(), 0);
        now += 1000000;
        store.refresh();
        QCOMPARE(store.topic("GameStatus").validity, QString("stale"));
        QCOMPARE(store.topic("GameStatus").reason, QString("timeout"));
        QCOMPARE(store.topic("GameStatus").fields["current_stage"].toInt(), 4);
        QCOMPARE(changed.count(), 1);
        store.refresh();
        QCOMPARE(changed.count(), 1);
        // UTC 即使跳变，也不影响使用单调时间计算的新鲜度。
        const auto decoded = ly27::MessageCodec::decode("GameStatus", QByteArray::fromHex("2804"));
        QVERIFY(store.apply(decoded, 1, now));
        QVERIFY(store.topic("GameStatus").isFresh());
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(100);
        QVERIFY(send(store, "RobotDynamicStatus", dynamic, now));
        QCOMPARE(*store.topic("RobotDynamicStatus").ttlMs, qint64(500));
    }

    void invalidMessagesRetainedReconnectAndNewSession()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("old"); store.setConnected(true);
        ly27::wire::GameStatus game;
        game.set_current_stage(4);
        QVERIFY(send(store, "GameStatus", game, now, true));
        QCOMPARE(store.topic("GameStatus").validity, QString("retained"));
        QVERIFY(!store.topic("GameStatus").isFresh());
        QVERIFY(send(store, "GameStatus", game, now));
        QVERIFY(store.topic("GameStatus").isFresh());
        QSignalSpy changed(&store, &ly27::StateStore::changed);
        QVERIFY(!store.apply(ly27::MessageCodec::decode("GameStatus", QByteArray::fromHex("80")), 1, now));
        QVERIFY(!store.apply(ly27::MessageCodec::decode("RadarInfoToClient", {}), 1, now));
        QVERIFY(!store.apply(ly27::MessageCodec::decode("AssemblyCommand", {}), 1, now));
        const auto good = ly27::MessageCodec::decode("GameStatus", QByteArray::fromHex("2801"));
        QVERIFY(!store.apply(good, 1, now - 1));
        QVERIFY(!store.apply(good, 1, now + 1));
        QCOMPARE(changed.count(), 0);
        QCOMPARE(store.topic("GameStatus").fields["current_stage"].toInt(), 4);
        store.setConnected(false);
        QCOMPARE(store.topic("GameStatus").validity, QString("disconnected"));
        QVERIFY(!store.apply(good, 1, now));
        store.setConnected(true);
        QCOMPARE(store.topic("GameStatus").validity, QString("stale"));
        QCOMPARE(store.topic("GameStatus").reason, QString("previous_connection"));
        QVERIFY(send(store, "GameStatus", game, now));
        QVERIFY(store.topic("GameStatus").isFresh());
        store.reset("new");
        QCOMPARE(store.topic("GameStatus").validity, QString("unknown"));
        QVERIFY(store.topic("GameStatus").fields.isEmpty());
        QCOMPARE(store.snapshot()["session_id"].toString(), QString("new"));
        QVERIFY(store.snapshot()["recent_notifications"].toArray().isEmpty());
        now += 1000000;
        store.reset("third"); store.setConnected(true);
        QVERIFY(!store.apply(good, 1, now - 1));
    }

    void blueTeamHealthMappingAndSourceSelection()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(ly27::Role::Infantry, ly27::Team::Blue), nullptr, [&now] { return now; });
        store.reset("blue"); store.setConnected(true);
        ly27::wire::GlobalUnitStatus units;
        for (int hp : {10, 20, 30, 40, 50, 60, 70, 0}) units.add_robot_health(hp);
        QVERIFY(send(store, "GlobalUnitStatus", units, now));
        QCOMPARE(store.robotHealth(101).fields["current_health"].toInt(), 10);
        QCOMPARE(store.robotHealth(103).fields["current_health"].toInt(), 20);
        QCOMPARE(store.robotHealth(1).fields["current_health"].toInt(), 50);
        QCOMPARE(store.robotHealth(7).fields["current_health"].toInt(-1), 0);
        QVERIFY(store.robotHealth(7).isFresh());
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(199);
        QVERIFY(send(store, "RobotDynamicStatus", dynamic, now));
        QCOMPARE(store.robotHealth(103).fields["current_health"].toInt(), 199);
        QCOMPARE(store.robotHealth(103).source["topic"].toString(), QString("RobotDynamicStatus"));
        now += 500000000;
        QVERIFY(store.robotHealth(103).isFresh());
        QCOMPARE(store.robotHealth(103).fields["current_health"].toInt(), 20);
        QCOMPARE(store.robotHealth(103).source["topic"].toString(), QString("GlobalUnitStatus"));
        // 不完整的血量数组不按猜测的顺序映射，也不保留上帧的映射。
        units.clear_robot_health();
        units.add_robot_health(10);
        QVERIFY(send(store, "GlobalUnitStatus", units, now));
        QCOMPARE(store.robotHealth(101).validity, QString("unknown"));
        QVERIFY(!store.robotHealth(103).isFresh());
    }

    void robotOwnershipAndCommander()
    {
        qint64 now = 1000000000;
        ly27::StateStore infantry(identity(), nullptr, [&now] { return now; });
        infantry.reset("infantry"); infantry.setConnected(true);
        ly27::wire::RobotStaticStatus stat;
        stat.set_robot_id(104);
        stat.set_max_health(400);
        QVERIFY(send(infantry, "RobotStaticStatus", stat, now));
        QCOMPARE(infantry.robotTopic(104, "RobotStaticStatus").fields["max_health"].toInt(), 400);
        QCOMPARE(infantry.robotTopic(3, "RobotStaticStatus").validity, QString("unknown"));
        QCOMPARE(infantry.snapshot()["self"].toObject()["robot_id"].toInt(), 3);
        stat.set_robot_id(0xffffffffu);
        QVERIFY(send(infantry, "RobotStaticStatus", stat, now));
        QCOMPARE(infantry.robotTopic(3, "RobotStaticStatus").validity, QString("unknown"));
        ly27::StateStore commander(identity(ly27::Role::Commander), nullptr, [&now] { return now; });
        commander.reset("commander"); commander.setConnected(true);
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(20);
        QVERIFY(send(commander, "RobotDynamicStatus", dynamic, now));
        QVERIFY(commander.snapshot()["self"].isNull());
        QVERIFY(commander.snapshot()["robots"].toObject().isEmpty());
        QVERIFY(commander.topic("RobotDynamicStatus").isFresh());
        ly27::wire::RobotPosition position;
        position.set_robot_id(107);
        position.set_x(0);
        position.set_y(12.5f);
        QVERIFY(send(commander, "RobotPosition", position, now));
        QCOMPARE(commander.robotTopic(107, "RobotPosition").fields["x"].toDouble(-1), 0.0);
        QCOMPARE(commander.robotTopic(107, "RobotPosition").fields["y"].toDouble(), 12.5);
        QVERIFY(!commander.snapshot()["role"].toObject().contains("assembly"));

        ly27::StateStore heavy(identity(ly27::Role::Heavy), nullptr, [&now] { return now; });
        heavy.reset("heavy"); heavy.setConnected(true);
        ly27::wire::TechCoreMotionStateSync assembly;
        assembly.set_basic_state(2);
        QVERIFY(send(heavy, "TechCoreMotionStateSync", assembly, now));
        QCOMPARE(heavy.snapshot()["role"].toObject()["assembly"].toObject()["fields"].toObject()["basic_state"].toInt(), 2);
        QVERIFY(heavy.snapshot()["topics"].toObject().contains("RuneStatusSync"));
    }

    void independentBuffsAndBoundedNotifications()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("events"); store.setConnected(true);
        ly27::wire::Buff buff;
        buff.set_robot_id(3); buff.set_buff_type(1); buff.set_buff_left_time(1); buff.set_buff_level(50);
        QVERIFY(send(store, "Buff", buff, now));
        buff.set_buff_type(2); buff.set_buff_left_time(4);
        QVERIFY(send(store, "Buff", buff, now));
        buff.set_robot_id(4); buff.set_buff_type(1); buff.set_buff_left_time(4);
        QVERIFY(send(store, "Buff", buff, now));
        QSignalSpy changed(&store, &ly27::StateStore::changed);
        now += 1000000000;
        store.refresh();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(store.buff(3, 1).validity, QString("stale"));
        QVERIFY(store.buff(3, 2).isFresh());
        QVERIFY(store.buff(4, 1).isFresh());
        buff.set_buff_left_time(0);
        QVERIFY(send(store, "Buff", buff, now));
        QCOMPARE(store.buff(4, 1).validity, QString("stale"));
        ly27::wire::Event event;
        for (int i = 0; i < 140; ++i) {
            event.set_event_id(i);
            QVERIFY(send(store, "Event", event, now));
        }
        const auto history = store.snapshot()["recent_notifications"].toArray();
        QCOMPARE(history.size(), ly27::StateStore::HistoryLimit);
        QCOMPARE(history.first().toObject()["fields"].toObject()["event_id"].toInt(), 12);
        now += 60000000000;
        QCOMPARE(store.topic("Event").validity, QString("observed"));
        ly27::wire::SentryCtrlResult result;
        result.set_command_id(1); result.set_result_code(0);
        QVERIFY(send(store, "SentryCtrlResult", result, now));
        QCOMPARE(store.topic("SentryCtrlResult").validity, QString("observed"));
        ly27::wire::CustomByteBlock bytes;
        bytes.set_data("abc");
        QVERIFY(send(store, "CustomByteBlock", bytes, now));
        QCOMPARE(store.snapshot()["recent_notifications"].toArray().size(), ly27::StateStore::HistoryLimit);
        QCOMPARE(store.topic("CustomByteBlock").fields["data"].toString(), QString("YWJj"));
    }

    void timerEmitsExpiryWithoutIncomingMessages()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("timer"); store.setConnected(true);
        ly27::wire::GameStatus game;
        game.set_current_stage(4);
        QVERIFY(send(store, "GameStatus", game, now));
        QSignalSpy changed(&store, &ly27::StateStore::changed);
        now += 600000000;
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 1000);
        QCOMPARE(store.topic("GameStatus").validity, QString("stale"));
    }
};

QTEST_GUILESS_MAIN(StateTest)
#include "test_state.moc"
