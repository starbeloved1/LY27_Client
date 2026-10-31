#include "StateStore.hpp"
#include "rm2027.pb.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QtTest>
#include <limits>

namespace {
ly27::TerminalIdentity identity(ly27::Role role = ly27::Role::Infantry,
                                ly27::Team team = ly27::Team::Red)
{
    ly27::TerminalIdentity result;
    result.role = role;
    result.team = team;
    result.stationId = "models-test";
    if (role != ly27::Role::Commander)
        result.robotId = (role == ly27::Role::Heavy ? 1 : 3) + (team == ly27::Team::Blue ? 100 : 0);
    return result;
}

bool send(ly27::StateStore &store, const google::protobuf::Message &message, qint64 now, bool retained = false)
{
    const auto topic = QString::fromStdString(message.GetDescriptor()->name());
    const auto decoded = ly27::MessageCodec::decode(topic, QByteArray::fromStdString(message.SerializeAsString()));
    return store.apply(decoded, 1700000000000, now, retained);
}
} // 命名空间

class StateModelsTest : public QObject {
    Q_OBJECT
private slots:
    void matchPresenceAndExactEconomy()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("match"); store.setConnected(true);
        QVERIFY(!store.matchState().stage.value);
        QVERIFY(!store.self()->currentHealth.value);
        QVERIFY(!store.robot(2));
        ly27::wire::GameStatus game;
        game.set_current_round(1); game.set_total_rounds(3);
        game.set_current_stage(4); game.set_is_paused(false);
        game.set_red_score(0); game.set_blue_score(2);
        game.set_stage_countdown_sec(-1); game.set_stage_elapsed_sec(12);
        game.set_game_result(0); game.set_end_reason(0);
        QVERIFY(send(store, game, now));
        const auto match = store.matchState();
        QCOMPARE(*match.currentRound.value, 1u);
        QCOMPARE(*match.totalRounds.value, 3u);
        QCOMPARE(*match.stage.value, ly27::GameStage::Battle);
        QVERIFY(*match.inBattle.value);
        QCOMPARE(*match.redScore.value, 0u);
        QCOMPARE(*match.blueScore.value, 2u);
        QVERIFY(match.paused.isFresh());
        QVERIFY(!*match.paused.value);
        QCOMPARE(*match.remainingSeconds.value, -1);
        QCOMPARE(*match.elapsedSeconds.value, 12);
        QCOMPARE(*match.result.value, 0u);
        QCOMPARE(match.stage.source.topic, QString("GameStatus"));
        QCOMPARE(*match.stage.source.round, 1u);

        ly27::wire::GlobalLogisticsStatus logistics;
        // 超过 double 的精确整数范围，必须从 Protobuf 原值读取。
        constexpr quint64 economy = Q_UINT64_C(9007199254740993);
        logistics.set_total_economy_obtained(economy);
        logistics.set_remaining_economy(0); logistics.set_tech_level(3); logistics.set_encryption_level(2);
        QVERIFY(send(store, logistics, now));
        const auto data = store.globalState().logistics;
        QCOMPARE(*data.totalEconomyObtained.value, economy);
        QCOMPARE(*data.remainingEconomy.value, 0u);
        QCOMPARE(*data.techLevel.value, 3u);
        QCOMPARE(*data.encryptionLevel.value, 2u);
        game.Clear(); game.set_is_paused(true);
        QVERIFY(send(store, game, now));
        QVERIFY(!store.matchState().stage.value);
        QVERIFY(!store.matchState().currentRound.value);
        QVERIFY(!store.matchState().inBattle.value);
        QCOMPARE(store.snapshot()["round"].toInt(), 1);
    }

    void robotAggregatesTopicsAndTracksIndependentSources()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("robot"); store.setConnected(true);
        ly27::wire::RobotStaticStatus stat;
        stat.set_robot_id(3); stat.set_robot_type(3); stat.set_alive_state(1);
        stat.set_connection_state(1); stat.set_field_state(0); stat.set_level(2);
        stat.set_max_health(250); stat.set_max_heat(100); stat.set_heat_cooldown_rate(12.5f);
        stat.set_max_power(80); stat.set_max_buffer_energy(60); stat.set_max_chassis_energy(70);
        stat.set_performance_system_shooter(1); stat.set_performance_system_chassis(2);
        QVERIFY(send(store, stat, now));
        now += 100000000;
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(125); dynamic.set_current_heat(20.5f);
        dynamic.set_remaining_ammo(0); dynamic.set_can_remote_heal(false); dynamic.set_can_remote_ammo(true);
        dynamic.set_current_buffer_energy(30); dynamic.set_current_experience(20);
        dynamic.set_experience_for_upgrade(100); dynamic.set_total_projectiles_fired(12);
        dynamic.set_last_projectile_fire_rate(23.5f); dynamic.set_is_out_of_combat(false);
        dynamic.set_out_of_combat_countdown(5);
        QVERIFY(send(store, dynamic, now));
        const auto robot = *store.self();
        QCOMPARE(robot.robotId, 3);
        QCOMPARE(robot.team, ly27::Team::Red);
        QCOMPARE(*robot.reportedRobotId.value, 3u);
        QCOMPARE(*robot.currentHealth.value, 125u);
        QCOMPARE(*robot.maxHealth.value, 250u);
        QCOMPARE(*robot.currentHeat.value, 20.5f);
        QCOMPARE(*robot.lastProjectileSpeed.value, 23.5f);
        QCOMPARE(*robot.heatCooldownPerSecond.value, 12.5f);
        QCOMPARE(*robot.maxPower.value, 80u);
        QCOMPARE(*robot.currentBufferEnergy.value, 30u);
        QCOMPARE(*robot.maxBufferEnergy.value, 60u);
        QCOMPARE(*robot.maxChassisEnergy.value, 70u);
        QCOMPARE(*robot.currentExperience.value, 20u);
        QCOMPARE(*robot.experienceForUpgrade.value, 100u);
        QCOMPARE(*robot.totalProjectilesFired.value, 12u);
        QCOMPARE(*robot.outOfCombatCountdown.value, 5u);
        QVERIFY(!*robot.outOfCombat.value);
        QCOMPARE(*robot.lifeState.value, ly27::RobotLifeState::Alive);
        QVERIFY(*robot.alive.value);
        QCOMPARE(*robot.remainingAmmo.value, 0u);
        QVERIFY(robot.canRemoteHeal.isFresh());
        QVERIFY(!*robot.canRemoteHeal.value);
        QVERIFY(*robot.canRemoteAmmo.value);
        QCOMPARE(*robot.healthRatio.value, 0.5);
        QCOMPARE(*robot.heatRatio.value, 0.205);
        QVERIFY(robot.healthRatio.isFresh());
        QCOMPARE(robot.healthRatio.inputs.size(), 2);
        QCOMPARE(robot.currentHealth.source.topic, QString("RobotDynamicStatus"));
        QCOMPARE(robot.maxHealth.source.topic, QString("RobotStaticStatus"));
        QVERIFY(robot.currentHealth.source.receivedMonotonicNs != robot.maxHealth.source.receivedMonotonicNs);
        now += 500000000;
        ly27::wire::RobotModuleStatus modules;
        modules.set_power_manager(1); modules.set_armor(0); modules.set_modular_42mm(1);
        QVERIFY(send(store, modules, now));
        const auto expired = *store.self();
        QVERIFY(!expired.currentHealth.isFresh());
        QVERIFY(expired.maxHealth.isFresh());
        QVERIFY(!expired.healthRatio.isFresh());
        QCOMPARE(*expired.currentHealth.value, 125u);
        QCOMPARE(*expired.modules.powerManager.value, ly27::RobotModuleState::Online);
        QCOMPARE(*expired.modules.armor.value, ly27::RobotModuleState::Offline);
        QCOMPARE(*expired.modules.modular42mmPermission.value, 1u);
        QVERIFY(expired.modules.powerManager.isFresh());
        // 查询返回快照；保存下来的旧结构不会自行变更，需要收到通知后重新查询。
        QVERIFY(robot.currentHealth.isFresh());
    }

    void noInventedRatiosLifeOrNonFiniteHeat()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("invalid"); store.setConnected(true);
        ly27::wire::RobotStaticStatus stat;
        stat.set_max_health(0); stat.set_max_heat(0); stat.set_alive_state(0);
        QVERIFY(send(store, stat, now));
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(0); dynamic.set_current_heat(0);
        QVERIFY(send(store, dynamic, now));
        QVERIFY(store.self()->currentHealth.isFresh());
        QCOMPARE(*store.self()->currentHealth.value, 0u);
        QVERIFY(!store.self()->healthRatio.value);
        QVERIFY(!store.self()->heatRatio.value);
        QVERIFY(!store.self()->alive.value);
        dynamic.set_current_heat(std::numeric_limits<float>::quiet_NaN());
        QVERIFY(send(store, dynamic, now));
        QVERIFY(!store.self()->currentHeat.value);
        QCOMPARE(store.self()->currentHeat.source.reason, QString("non_finite_value"));
        stat.set_alive_state(2);
        QVERIFY(send(store, stat, now));
        QVERIFY(store.self()->alive.isFresh());
        QVERIFY(!*store.self()->alive.value);
    }

    void globalFacilitiesMechanismsAndHealthFallback()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(ly27::Role::Infantry, ly27::Team::Blue), nullptr, [&now] { return now; });
        store.reset("global"); store.setConnected(true);
        ly27::wire::GlobalUnitStatus units;
        units.set_base_health(5000); units.set_base_status(0); units.set_base_shield(1);
        units.set_enemy_base_health(2000); units.set_enemy_base_status(2); units.set_enemy_base_shield(2);
        units.set_outpost_r_health(1000); units.set_outpost_r_status(3);
        units.set_outpost_b_health(900); units.set_outpost_b_status(4);
        units.set_total_damage_ally(100); units.set_total_damage_enemy(200);
        units.set_total_cp_ally(10); units.set_total_cp_enemy(20);
        for (int hp : {10, 20, 30, 40, 50, 60, 70, 0}) units.add_robot_health(hp);
        units.add_robot_bullets(-1);
        QVERIFY(send(store, units, now));
        const auto global = store.globalState();
        QCOMPARE(global.allyBase.team, ly27::Team::Blue);
        QCOMPARE(global.enemyBase.team, ly27::Team::Red);
        QCOMPARE(*global.allyBase.currentHealth.value, 5000u);
        QCOMPARE(*global.allyBase.controlState.value, 1u);
        QCOMPARE(*global.enemyBase.defenseState.value, 2u);
        QCOMPARE(*global.rOutpost.currentHealth.value, 1000u);
        QCOMPARE(*global.bOutpost.controlState.value, 4u);
        QCOMPARE(*global.allyDamage.value, 100u);
        QCOMPARE(*global.enemyDamage.value, 200u);
        QCOMPARE(*global.allyControlPoints.value, 10u);
        QCOMPARE(*global.enemyControlPoints.value, 20u);
        QCOMPARE(global.robotHealth.value->size(), 8);
        QCOMPARE(global.robotBullets.value->front(), -1);
        QCOMPARE(*store.self()->currentHealth.value, 20u);
        QCOMPARE(*store.robot(7)->currentHealth.value, 0u);
        QVERIFY(!store.robot(7)->alive.value);
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(199);
        QVERIFY(send(store, dynamic, now));
        QCOMPARE(*store.self()->currentHealth.value, 199u);
        now += 500000000;
        QCOMPARE(*store.self()->currentHealth.value, 20u);
        QCOMPARE(store.self()->currentHealth.source.topic, QString("GlobalUnitStatus"));
        QVERIFY(store.self()->currentHealth.isFresh());
        ly27::wire::GlobalSpecialMechanism mechanisms;
        mechanisms.add_mechanism_id(5); mechanisms.add_mechanism_time_sec(-1);
        QVERIFY(send(store, mechanisms, now));
        QCOMPARE(store.globalState().mechanisms.value->front().id, 5u);
        QCOMPARE(store.globalState().mechanisms.value->front().remainingSeconds, -1);
        mechanisms.add_mechanism_id(6);
        QVERIFY(send(store, mechanisms, now));
        QVERIFY(!store.globalState().mechanisms.value);
        QCOMPARE(store.globalState().mechanisms.source.reason, QString("mismatched_arrays"));
        mechanisms.Clear();
        QVERIFY(send(store, mechanisms, now));
        QVERIFY(store.globalState().mechanisms.isFresh());
        QVERIFY(store.globalState().mechanisms.value->isEmpty());
    }

    void respawnInjuryPerformanceBuffAndBinaryData()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("substates"); store.setConnected(true);
        ly27::wire::RobotRespawnStatus respawn;
        respawn.set_is_pending_respawn(true); respawn.set_total_respawn_progress(100);
        respawn.set_current_respawn_progress(30); respawn.set_can_free_respawn(false);
        respawn.set_gold_cost_for_respawn(0); respawn.set_can_pay_for_respawn(true);
        QVERIFY(send(store, respawn, now));
        QVERIFY(*store.self()->respawn.pending.value);
        QVERIFY(!*store.self()->respawn.canFreeRespawn.value);
        QVERIFY(*store.self()->respawn.canPay.value);
        QCOMPARE(*store.self()->respawn.goldCost.value, 0u);
        QCOMPARE(*store.self()->respawn.progressRatio.value, 0.3);
        ly27::wire::RobotInjuryStat injury;
        injury.set_total_damage(90); injury.set_collision_damage(10); injury.set_small_projectile_damage(20);
        injury.set_large_projectile_damage(30); injury.set_dart_splash_damage(5); injury.set_module_offline_damage(4);
        injury.set_offline_damage(6); injury.set_penalty_damage(7); injury.set_server_kill_damage(8); injury.set_killer_id(103);
        QVERIFY(send(store, injury, now));
        const auto damage = store.self()->injury;
        QCOMPARE(*damage.totalDamage.value, 90u);
        QCOMPARE(*damage.smallProjectileDamage.value, 20u);
        QCOMPARE(*damage.largeProjectileDamage.value, 30u);
        QCOMPARE(*damage.serverKillDamage.value, 8u);
        QCOMPARE(*damage.killerId.value, 103u);
        ly27::wire::RobotStaticStatus stat;
        stat.set_performance_system_shooter(1); stat.set_performance_system_chassis(2);
        QVERIFY(send(store, stat, now));
        ly27::wire::RobotPerformanceSelectionSync selection;
        selection.set_shooter(2); selection.set_chassis(1); selection.set_sentry_control(0);
        QVERIFY(send(store, selection, now));
        QCOMPARE(*store.self()->performance.shooter.value, 2u);
        QCOMPARE(*store.roleState().performance.sentryControl.value, 0u);
        now += 1000000000;
        QVERIFY(send(store, stat, now));
        now += 2000000000;
        QCOMPARE(*store.self()->performance.shooter.value, 1u);
        QCOMPARE(store.self()->performance.shooter.source.topic, QString("RobotStaticStatus"));
        ly27::wire::Buff buff;
        buff.set_robot_id(3); buff.set_buff_type(2); buff.set_buff_level(-50);
        buff.set_buff_max_time(10); buff.set_buff_left_time(1);
        QVERIFY(send(store, buff, now));
        const auto defense = store.self()->buffs.value(ly27::BuffType::Defense);
        QCOMPARE(*defense.level.value, -50);
        QCOMPARE(*defense.maximumSeconds.value, 10u);
        QVERIFY(defense.level.isFresh());
        ly27::wire::CustomByteBlock custom;
        custom.set_data(std::string("\0\xff\0", 3));
        QVERIFY(send(store, custom, now));
        QCOMPARE(*store.self()->customData.value, QByteArray::fromHex("00ff00"));
        custom.set_data("");
        QVERIFY(send(store, custom, now));
        QVERIFY(store.self()->customData.value.has_value());
        QVERIFY(store.self()->customData.value->isEmpty());
        now += 1000000000;
        QVERIFY(!store.self()->buffs.value(ly27::BuffType::Defense).level.isFresh());
    }

    void roleStateAndTypedNotifications()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(ly27::Role::Commander), nullptr, [&now] { return now; });
        store.reset("roles"); store.setConnected(true);
        QVERIFY(!store.self());
        ly27::wire::RobotPosition position;
        position.set_robot_id(107); position.set_x(0); position.set_y(12.5f); position.set_z(1); position.set_yaw(-30);
        QVERIFY(send(store, position, now));
        QCOMPARE(*store.robot(107)->position.x.value, 0.0f);
        QCOMPARE(*store.robot(107)->position.yawDegrees.value, -30.0f);
        ly27::wire::TechCoreMotionStateSync assembly;
        assembly.set_maximum_difficulty_level(3); assembly.set_basic_state(1); assembly.set_putin_state(2);
        assembly.set_move_state(3); assembly.set_rotate_state(4); assembly.set_enemy_core_status(5);
        assembly.set_remain_time_all(60); assembly.set_remain_time_step(10);
        QVERIFY(send(store, assembly, now));
        QCOMPARE(*store.roleState().assembly.maximumDifficulty.value, 3u);
        QCOMPARE(*store.roleState().assembly.putInState.value, 2u);
        QCOMPARE(*store.roleState().assembly.remainingStepSeconds.value, 10u);
        ly27::wire::RuneStatusSync rune;
        rune.set_rune_status(2); rune.set_activated_arms(3); rune.set_average_rings(8.5f);
        QVERIFY(send(store, rune, now));
        QCOMPARE(*store.roleState().rune.averageRings.value, 8.5f);
        ly27::wire::SentryStatusSync sentry;
        sentry.set_posture_id(2); sentry.set_is_weakened(false); sentry.set_is_powered(true);
        QVERIFY(send(store, sentry, now));
        QVERIFY(store.roleState().sentry.weakened.isFresh());
        QVERIFY(!*store.roleState().sentry.weakened.value);
        ly27::wire::DartSelectTargetStatusSync dart;
        dart.set_target_id(1); dart.set_open(2);
        QVERIFY(send(store, dart, now));
        QCOMPARE(*store.roleState().dart.openingState.value, 2u);
        ly27::wire::Event event;
        event.set_event_id(17); event.set_param("notice");
        QVERIFY(send(store, event, now));
        ly27::wire::PenaltyInfo penalty;
        penalty.set_penalty_type(1); penalty.set_penalty_effect_sec(10); penalty.set_total_penalty_num(2);
        QVERIFY(send(store, penalty, now));
        ly27::wire::MapClickInfo marker;
        marker.set_robot_id(std::string("\x01\x07\0", 3)); marker.set_mode(4); marker.set_type(1);
        marker.set_is_send_all(0); marker.set_map_x(1.5f); marker.set_map_y(0);
        QVERIFY(send(store, marker, now));
        QCOMPARE(*store.roleState().lastMapMarker.receiverIds.value, QByteArray::fromHex("010700"));
        QCOMPARE(*store.roleState().lastMapMarker.markerType.value, 4u);
        QCOMPARE(*store.roleState().lastMapMarker.markerMode.value, 1u);
        ly27::wire::RobotPathPlanInfo path;
        path.set_intention(3); path.set_start_pos_x(10); path.set_start_pos_y(20); path.set_sender_id(7);
        path.add_offset_x(-128); path.add_offset_y(127);
        QVERIFY(send(store, path, now));
        QCOMPARE(store.roleState().lastPathPlan.offsetsX.value->front(), -128);
        QCOMPARE(*store.roleState().lastPathPlan.startXDecimeters.value, 10u);
        ly27::wire::SentryCtrlResult result;
        result.set_command_id(12); result.set_result_code(0);
        QVERIFY(send(store, result, now));
        const auto notifications = store.notifications();
        QCOMPARE(notifications.size(), 5);
        QCOMPARE(*std::get<ly27::EventNotice>(notifications[0]).parameter.value, QString("notice"));
        QCOMPARE(*std::get<ly27::PenaltyState>(notifications[1]).type.value, 1u);
        QVERIFY(std::holds_alternative<ly27::MapMarkerState>(notifications[2]));
        QVERIFY(std::holds_alternative<ly27::PathPlanState>(notifications[3]));
        const auto last = std::get<ly27::SentryControlResult>(notifications[4]);
        QCOMPARE(*last.resultCode.value, 0u);
        QCOMPARE(last.resultCode.source.validity, ly27::StateValidity::Observed);
        QVERIFY(!last.resultCode.isFresh());
    }

    void roundChangeClearsOldStateButRetainedDoesNotSwitchRounds()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("rounds"); store.setConnected(true);
        ly27::wire::GameStatus game;
        game.set_current_round(1); game.set_current_stage(4);
        QVERIFY(send(store, game, now));
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(100);
        QVERIFY(send(store, dynamic, now));
        ly27::wire::GlobalLogisticsStatus logistics;
        logistics.set_remaining_economy(100);
        QVERIFY(send(store, logistics, now));
        ly27::wire::Event event;
        event.set_event_id(3);
        QVERIFY(send(store, event, now));
        ly27::wire::Buff buff;
        buff.set_robot_id(3); buff.set_buff_type(1); buff.set_buff_left_time(20);
        QVERIFY(send(store, buff, now));
        const auto previousTime = now;
        now += 1000000;
        game.set_current_round(2);
        QSignalSpy changed(&store, &ly27::StateStore::changed);
        QVERIFY(send(store, game, now));
        QCOMPARE(changed.count(), 1);
        QVERIFY(changed.front()[0].toString().isEmpty());
        QVERIFY(!store.self()->currentHealth.value);
        QVERIFY(!store.globalState().logistics.remainingEconomy.value);
        QVERIFY(store.self()->buffs.isEmpty());
        QVERIFY(store.notifications().isEmpty());
        QCOMPARE(store.snapshot()["session_id"].toString(), QString("rounds"));
        QVERIFY(store.snapshot()["connected"].toBool());
        QCOMPARE(store.matchState().currentRound.source.roundRevision, quint64(1));
        QVERIFY(!send(store, dynamic, previousTime));
        QVERIFY(send(store, dynamic, now));
        QCOMPARE(*store.self()->currentHealth.source.round, 2u);
        game.set_current_round(3);
        QVERIFY(send(store, game, now, true));
        QCOMPARE(store.snapshot()["round"].toInt(), 2);
        QVERIFY(store.self()->currentHealth.isFresh());
        QCOMPARE(store.matchState().currentRound.source.validity, ly27::StateValidity::Retained);
        QVERIFY(!store.matchState().currentRound.source.round);
        QVERIFY(send(store, game, now));
        QVERIFY(!store.self()->currentHealth.value);
    }

    void typedValuesFollowDisconnectReconnectAndReset()
    {
        qint64 now = 1000000000;
        ly27::StateStore store(identity(), nullptr, [&now] { return now; });
        store.reset("first"); store.setConnected(true);
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_current_health(100);
        QVERIFY(send(store, dynamic, now));
        store.setConnected(false);
        QCOMPARE(store.self()->currentHealth.source.validity, ly27::StateValidity::Disconnected);
        QCOMPARE(*store.self()->currentHealth.value, 100u);
        store.setConnected(true);
        QCOMPARE(store.self()->currentHealth.source.validity, ly27::StateValidity::Stale);
        QCOMPARE(store.self()->currentHealth.source.reason, QString("previous_connection"));
        QVERIFY(send(store, dynamic, now));
        QVERIFY(store.self()->currentHealth.isFresh());
        store.reset("second");
        QVERIFY(!store.self()->currentHealth.value);
        QVERIFY(!store.self()->healthRatio.value);
        QVERIFY(store.notifications().isEmpty());
    }
};

QTEST_GUILESS_MAIN(StateModelsTest)
#include "test_state_models.moc"
