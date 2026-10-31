#include "StateStore.hpp"
#include "rm2027.pb.h"

#include <QThread>
#include <cmath>
#include <type_traits>
#include <utility>

// 管理字段分发
namespace ly27 {
namespace {
template<typename T>
StateField<T> field(bool present, T value, StateSource source)
{
    if (!present) {
        source.validity = StateValidity::Unknown;
        if (source.received) source.reason = "missing_field";
        return {{}, std::move(source)};
    }
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
            source.validity = StateValidity::Unknown;
            source.reason = "non_finite_value";
            return {{}, std::move(source)};
        }
    }
    return {std::move(value), std::move(source)};
}

template<typename T>
QList<T> list(const google::protobuf::RepeatedField<T> &values)
{
    return {values.begin(), values.end()};
}

template<typename T>
StateField<T> choose(const StateField<T> &preferred, const StateField<T> &fallback)
{
    if (preferred.isFresh()) return preferred;
    if (fallback.isFresh()) return fallback;
    if (preferred.value && (!fallback.value || preferred.source.receivedMonotonicNs >= fallback.source.receivedMonotonicNs))
        return preferred;
    return fallback;
}

template<typename N, typename D>
DerivedState<double> ratio(const StateField<N> &numerator, const StateField<D> &denominator)
{
    DerivedState<double> result;
    result.inputs = {numerator.source, denominator.source};
    if (numerator.value && denominator.value && *numerator.value >= 0 && *denominator.value > 0)
        result.value = static_cast<double>(*numerator.value) / static_cast<double>(*denominator.value);
    return result;
}

PerformanceState performance(const wire::RobotPerformanceSelectionSync *p, const StateSource &s)
{
    PerformanceState result;
    if (!p) return result;
    result.shooter = field(p->has_shooter(), p->shooter(), s);
    result.chassis = field(p->has_chassis(), p->chassis(), s);
    result.sentryControl = field(p->has_sentry_control(), p->sentry_control(), s);
    return result;
}

PenaltyState penalty(const wire::PenaltyInfo *p, const StateSource &s)
{
    PenaltyState result;
    if (!p) return result;
    result.type = field(p->has_penalty_type(), p->penalty_type(), s);
    result.effectSeconds = field(p->has_penalty_effect_sec(), p->penalty_effect_sec(), s);
    result.totalCount = field(p->has_total_penalty_num(), p->total_penalty_num(), s);
    return result;
}

EventNotice eventNotice(const wire::Event *p, const StateSource &s)
{
    EventNotice result;
    if (!p) return result;
    result.id = field(p->has_event_id(), p->event_id(), s);
    result.parameter = field(p->has_param(), QString::fromStdString(p->param()), s);
    return result;
}

MapMarkerState marker(const wire::MapClickInfo *p, const StateSource &s)
{
    MapMarkerState result;
    if (!p) return result;
    result.sendRange = field(p->has_is_send_all(), p->is_send_all(), s);
    result.receiverIds = field(p->has_robot_id(), QByteArray::fromStdString(p->robot_id()), s);
    result.markerType = field(p->has_mode(), p->mode(), s);
    result.enemyId = field(p->has_enemy_id(), p->enemy_id(), s);
    result.ascii = field(p->has_ascii(), p->ascii(), s);
    result.markerMode = field(p->has_type(), p->type(), s);
    result.x = field(p->has_map_x(), p->map_x(), s);
    result.y = field(p->has_map_y(), p->map_y(), s);
    return result;
}

PathPlanState path(const wire::RobotPathPlanInfo *p, const StateSource &s)
{
    PathPlanState result;
    if (!p) return result;
    result.intention = field(p->has_intention(), p->intention(), s);
    result.startXDecimeters = field(p->has_start_pos_x(), p->start_pos_x(), s);
    result.startYDecimeters = field(p->has_start_pos_y(), p->start_pos_y(), s);
    result.offsetsX = field(true, list(p->offset_x()), s);
    result.offsetsY = field(true, list(p->offset_y()), s);
    result.senderId = field(p->has_sender_id(), p->sender_id(), s);
    return result;
}

SentryControlResult sentryResult(const wire::SentryCtrlResult *p, const StateSource &s)
{
    SentryControlResult result;
    if (!p) return result;
    result.commandId = field(p->has_command_id(), p->command_id(), s);
    result.resultCode = field(p->has_result_code(), p->result_code(), s);
    return result;
}
} // 命名空间

MatchState StateStore::matchState() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    MatchState result;
    const auto sample = m_topics.value("GameStatus");
    const auto *p = dynamic_cast<const wire::GameStatus *>(sample.message.get());
    if (!p) return result;
    const auto s = source(sample, m_clock());
    result.currentRound = field(p->has_current_round(), p->current_round(), s);
    result.totalRounds = field(p->has_total_rounds(), p->total_rounds(), s);
    result.redScore = field(p->has_red_score(), p->red_score(), s);
    result.blueScore = field(p->has_blue_score(), p->blue_score(), s);
    result.stage = field(p->has_current_stage(), static_cast<GameStage>(p->current_stage()), s);
    result.remainingSeconds = field(p->has_stage_countdown_sec(), p->stage_countdown_sec(), s);
    result.elapsedSeconds = field(p->has_stage_elapsed_sec(), p->stage_elapsed_sec(), s);
    result.paused = field(p->has_is_paused(), p->is_paused(), s);
    result.result = field(p->has_game_result(), p->game_result(), s);
    result.endReason = field(p->has_end_reason(), p->end_reason(), s);
    result.inBattle = field(p->has_current_stage() && p->current_stage() <= 5,
                            p->current_stage() == static_cast<quint32>(GameStage::Battle), s);
    return result;
}

GlobalState StateStore::globalState() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto now = m_clock();
    GlobalState result;
    result.allyBase.team = m_identity.team;
    result.enemyBase.team = m_identity.team == Team::Red ? Team::Blue : Team::Red;
    auto sample = m_topics.value("GlobalUnitStatus");
    if (const auto *p = dynamic_cast<const wire::GlobalUnitStatus *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.allyBase.currentHealth = field(p->has_base_health(), p->base_health(), s);
        result.allyBase.defenseState = field(p->has_base_status(), p->base_status(), s);
        result.allyBase.controlState = field(p->has_base_shield(), p->base_shield(), s);
        result.enemyBase.currentHealth = field(p->has_enemy_base_health(), p->enemy_base_health(), s);
        result.enemyBase.defenseState = field(p->has_enemy_base_status(), p->enemy_base_status(), s);
        result.enemyBase.controlState = field(p->has_enemy_base_shield(), p->enemy_base_shield(), s);
        result.rOutpost.currentHealth = field(p->has_outpost_r_health(), p->outpost_r_health(), s);
        result.rOutpost.controlState = field(p->has_outpost_r_status(), p->outpost_r_status(), s);
        result.bOutpost.currentHealth = field(p->has_outpost_b_health(), p->outpost_b_health(), s);
        result.bOutpost.controlState = field(p->has_outpost_b_status(), p->outpost_b_status(), s);
        result.robotHealth = field(true, list(p->robot_health()), s);
        result.robotBullets = field(true, list(p->robot_bullets()), s);
        result.allyDamage = field(p->has_total_damage_ally(), p->total_damage_ally(), s);
        result.enemyDamage = field(p->has_total_damage_enemy(), p->total_damage_enemy(), s);
        result.allyControlPoints = field(p->has_total_cp_ally(), p->total_cp_ally(), s);
        result.enemyControlPoints = field(p->has_total_cp_enemy(), p->total_cp_enemy(), s);
    }
    sample = m_topics.value("GlobalLogisticsStatus");
    if (const auto *p = dynamic_cast<const wire::GlobalLogisticsStatus *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.logistics.remainingEconomy = field(p->has_remaining_economy(), p->remaining_economy(), s);
        result.logistics.totalEconomyObtained = field<quint64>(p->has_total_economy_obtained(), p->total_economy_obtained(), s);
        result.logistics.techLevel = field(p->has_tech_level(), p->tech_level(), s);
        result.logistics.encryptionLevel = field(p->has_encryption_level(), p->encryption_level(), s);
    }
    sample = m_topics.value("GlobalSpecialMechanism");
    if (const auto *p = dynamic_cast<const wire::GlobalSpecialMechanism *>(sample.message.get())) {
        auto s = source(sample, now);
        if (p->mechanism_id_size() == p->mechanism_time_sec_size()) {
            QList<SpecialMechanism> mechanisms;
            for (int i = 0; i < p->mechanism_id_size(); ++i)
                mechanisms.append({p->mechanism_id(i), p->mechanism_time_sec(i)});
            result.mechanisms = field(true, mechanisms, s);
        } else {
            s.validity = StateValidity::Unknown;
            s.reason = "mismatched_arrays";
            result.mechanisms.source = s;
        }
    }
    return result;
}

std::optional<RobotState> StateStore::robot(int robotId) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (robotId != 1 && robotId != 3 && robotId != 4 && robotId != 7 &&
        robotId != 101 && robotId != 103 && robotId != 104 && robotId != 107) return {};
    const auto now = m_clock();
    const auto records = m_robots.value(robotId);
    RobotState result;
    result.robotId = robotId;
    result.team = robotId >= 100 ? Team::Blue : Team::Red;

    auto sample = healthSample(robotId, now);
    result.currentHealth = field(sample.projectedHealth.has_value(), sample.projectedHealth.value_or(0), source(sample, now));
    sample = records.value("RobotStaticStatus");
    if (const auto *p = dynamic_cast<const wire::RobotStaticStatus *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.reportedRobotId = field(p->has_robot_id(), p->robot_id(), s);
        result.robotType = field(p->has_robot_type(), p->robot_type(), s);
        result.connectionState = field(p->has_connection_state(), p->connection_state(), s);
        result.fieldState = field(p->has_field_state(), p->field_state(), s);
        result.lifeState = field(p->has_alive_state(), static_cast<RobotLifeState>(p->alive_state()), s);
        result.alive = field(p->has_alive_state() && (p->alive_state() == 1 || p->alive_state() == 2), p->alive_state() == 1, s);
        result.level = field(p->has_level(), p->level(), s);
        result.maxHealth = field(p->has_max_health(), p->max_health(), s);
        result.maxHeat = field(p->has_max_heat(), p->max_heat(), s);
        result.heatCooldownPerSecond = field(p->has_heat_cooldown_rate(), p->heat_cooldown_rate(), s);
        result.maxPower = field(p->has_max_power(), p->max_power(), s);
        result.maxBufferEnergy = field(p->has_max_buffer_energy(), p->max_buffer_energy(), s);
        result.maxChassisEnergy = field(p->has_max_chassis_energy(), p->max_chassis_energy(), s);
        result.performance.shooter = field(p->has_performance_system_shooter(), p->performance_system_shooter(), s);
        result.performance.chassis = field(p->has_performance_system_chassis(), p->performance_system_chassis(), s);
    }
    sample = records.value("RobotDynamicStatus");
    if (const auto *p = dynamic_cast<const wire::RobotDynamicStatus *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.currentHeat = field(p->has_current_heat(), p->current_heat(), s);
        result.lastProjectileSpeed = field(p->has_last_projectile_fire_rate(), p->last_projectile_fire_rate(), s);
        result.currentBufferEnergy = field(p->has_current_buffer_energy(), p->current_buffer_energy(), s);
        result.currentExperience = field(p->has_current_experience(), p->current_experience(), s);
        result.experienceForUpgrade = field(p->has_experience_for_upgrade(), p->experience_for_upgrade(), s);
        result.totalProjectilesFired = field(p->has_total_projectiles_fired(), p->total_projectiles_fired(), s);
        result.remainingAmmo = field(p->has_remaining_ammo(), p->remaining_ammo(), s);
        result.outOfCombat = field(p->has_is_out_of_combat(), p->is_out_of_combat(), s);
        result.outOfCombatCountdown = field(p->has_out_of_combat_countdown(), p->out_of_combat_countdown(), s);
        result.canRemoteHeal = field(p->has_can_remote_heal(), p->can_remote_heal(), s);
        result.canRemoteAmmo = field(p->has_can_remote_ammo(), p->can_remote_ammo(), s);
    }
    sample = records.value("RobotModuleStatus");
    if (const auto *p = dynamic_cast<const wire::RobotModuleStatus *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.modules.powerManager = field(p->has_power_manager(), static_cast<RobotModuleState>(p->power_manager()), s);
        result.modules.rfid = field(p->has_rfid(), static_cast<RobotModuleState>(p->rfid()), s);
        result.modules.lightStrip = field(p->has_light_strip(), static_cast<RobotModuleState>(p->light_strip()), s);
        result.modules.smallShooter = field(p->has_small_shooter(), static_cast<RobotModuleState>(p->small_shooter()), s);
        result.modules.bigShooter = field(p->has_big_shooter(), static_cast<RobotModuleState>(p->big_shooter()), s);
        result.modules.uwb = field(p->has_uwb(), static_cast<RobotModuleState>(p->uwb()), s);
        result.modules.armor = field(p->has_armor(), static_cast<RobotModuleState>(p->armor()), s);
        result.modules.videoTransmission = field(p->has_video_transmission(), static_cast<RobotModuleState>(p->video_transmission()), s);
        result.modules.capacitor = field(p->has_capacitor(), static_cast<RobotModuleState>(p->capacitor()), s);
        result.modules.mainController = field(p->has_main_controller(), static_cast<RobotModuleState>(p->main_controller()), s);
        result.modules.modular42mmPermission = field(p->has_modular_42mm(), p->modular_42mm(), s);
    }
    sample = records.value("RobotPosition");
    if (const auto *p = dynamic_cast<const wire::RobotPosition *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.position.x = field(p->has_x(), p->x(), s);
        result.position.y = field(p->has_y(), p->y(), s);
        result.position.z = field(p->has_z(), p->z(), s);
        result.position.yawDegrees = field(p->has_yaw(), p->yaw(), s);
        result.position.reportedRobotId = field(p->has_robot_id(), p->robot_id(), s);
    }
    sample = records.value("RobotRespawnStatus");
    if (const auto *p = dynamic_cast<const wire::RobotRespawnStatus *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.respawn.pending = field(p->has_is_pending_respawn(), p->is_pending_respawn(), s);
        result.respawn.totalProgress = field(p->has_total_respawn_progress(), p->total_respawn_progress(), s);
        result.respawn.currentProgress = field(p->has_current_respawn_progress(), p->current_respawn_progress(), s);
        result.respawn.canFreeRespawn = field(p->has_can_free_respawn(), p->can_free_respawn(), s);
        result.respawn.goldCost = field(p->has_gold_cost_for_respawn(), p->gold_cost_for_respawn(), s);
        result.respawn.canPay = field(p->has_can_pay_for_respawn(), p->can_pay_for_respawn(), s);
    }
    sample = records.value("RobotInjuryStat");
    if (const auto *p = dynamic_cast<const wire::RobotInjuryStat *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.injury.totalDamage = field(p->has_total_damage(), p->total_damage(), s);
        result.injury.collisionDamage = field(p->has_collision_damage(), p->collision_damage(), s);
        result.injury.smallProjectileDamage = field(p->has_small_projectile_damage(), p->small_projectile_damage(), s);
        result.injury.largeProjectileDamage = field(p->has_large_projectile_damage(), p->large_projectile_damage(), s);
        result.injury.dartSplashDamage = field(p->has_dart_splash_damage(), p->dart_splash_damage(), s);
        result.injury.moduleOfflineDamage = field(p->has_module_offline_damage(), p->module_offline_damage(), s);
        result.injury.offlineDamage = field(p->has_offline_damage(), p->offline_damage(), s);
        result.injury.penaltyDamage = field(p->has_penalty_damage(), p->penalty_damage(), s);
        result.injury.serverKillDamage = field(p->has_server_kill_damage(), p->server_kill_damage(), s);
        result.injury.killerId = field(p->has_killer_id(), p->killer_id(), s);
    }
    sample = records.value("PenaltyInfo");
    result.penalty = penalty(dynamic_cast<const wire::PenaltyInfo *>(sample.message.get()), source(sample, now));
    sample = records.value("RobotPerformanceSelectionSync");
    const auto selection = performance(dynamic_cast<const wire::RobotPerformanceSelectionSync *>(sample.message.get()), source(sample, now));
    result.performance.shooter = choose(selection.shooter, result.performance.shooter);
    result.performance.chassis = choose(selection.chassis, result.performance.chassis);
    result.performance.sentryControl = selection.sentryControl;
    for (int type = 1; type <= 7; ++type) {
        sample = records.value("Buff/" + QString::number(type));
        const auto *p = dynamic_cast<const wire::Buff *>(sample.message.get());
        if (!p) continue;
        const auto s = source(sample, now);
        BuffState buff;
        buff.robotId = field(p->has_robot_id(), p->robot_id(), s);
        buff.type = field(p->has_buff_type(), static_cast<BuffType>(p->buff_type()), s);
        buff.level = field(p->has_buff_level(), p->buff_level(), s);
        buff.maximumSeconds = field(p->has_buff_max_time(), p->buff_max_time(), s);
        buff.reportedRemainingSeconds = field(p->has_buff_left_time(), p->buff_left_time(), s);
        result.buffs.insert(static_cast<BuffType>(type), buff);
    }
    sample = records.value("CustomByteBlock");
    if (const auto *p = dynamic_cast<const wire::CustomByteBlock *>(sample.message.get()))
        result.customData = field(p->has_data(), QByteArray::fromStdString(p->data()), source(sample, now));
    result.healthRatio = ratio(result.currentHealth, result.maxHealth);
    result.heatRatio = ratio(result.currentHeat, result.maxHeat);
    result.respawn.progressRatio = ratio(result.respawn.currentProgress, result.respawn.totalProgress);
    return result;
}

std::optional<RobotState> StateStore::self() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return m_identity.robotId ? robot(*m_identity.robotId) : std::nullopt;
}

RoleState StateStore::roleState() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto now = m_clock();
    RoleState result;
    result.role = m_identity.role;
    auto sample = m_topics.value("TechCoreMotionStateSync");
    if (const auto *p = dynamic_cast<const wire::TechCoreMotionStateSync *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.assembly.maximumDifficulty = field(p->has_maximum_difficulty_level(), p->maximum_difficulty_level(), s);
        result.assembly.basicState = field(p->has_basic_state(), p->basic_state(), s);
        result.assembly.putInState = field(p->has_putin_state(), p->putin_state(), s);
        result.assembly.moveState = field(p->has_move_state(), p->move_state(), s);
        result.assembly.rotateState = field(p->has_rotate_state(), p->rotate_state(), s);
        result.assembly.enemyCoreStatus = field(p->has_enemy_core_status(), p->enemy_core_status(), s);
        result.assembly.remainingTotalSeconds = field(p->has_remain_time_all(), p->remain_time_all(), s);
        result.assembly.remainingStepSeconds = field(p->has_remain_time_step(), p->remain_time_step(), s);
    }
    sample = m_topics.value("RobotPerformanceSelectionSync");
    result.performance = performance(dynamic_cast<const wire::RobotPerformanceSelectionSync *>(sample.message.get()), source(sample, now));
    if (const auto own = self()) result.performance = own->performance;
    sample = m_topics.value("RuneStatusSync");
    if (const auto *p = dynamic_cast<const wire::RuneStatusSync *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.rune.status = field(p->has_rune_status(), p->rune_status(), s);
        result.rune.activatedArms = field(p->has_activated_arms(), p->activated_arms(), s);
        result.rune.averageRings = field(p->has_average_rings(), p->average_rings(), s);
    }
    sample = m_topics.value("SentryStatusSync");
    if (const auto *p = dynamic_cast<const wire::SentryStatusSync *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.sentry.posture = field(p->has_posture_id(), p->posture_id(), s);
        result.sentry.weakened = field(p->has_is_weakened(), p->is_weakened(), s);
        result.sentry.powered = field(p->has_is_powered(), p->is_powered(), s);
    }
    sample = m_topics.value("DartSelectTargetStatusSync");
    if (const auto *p = dynamic_cast<const wire::DartSelectTargetStatusSync *>(sample.message.get())) {
        const auto s = source(sample, now);
        result.dart.targetId = field(p->has_target_id(), p->target_id(), s);
        result.dart.openingState = field(p->has_open(), p->open(), s);
    }
    sample = m_topics.value("SentryCtrlResult");
    result.lastSentryResult = sentryResult(dynamic_cast<const wire::SentryCtrlResult *>(sample.message.get()), source(sample, now));
    sample = m_topics.value("MapClickInfo");
    result.lastMapMarker = marker(dynamic_cast<const wire::MapClickInfo *>(sample.message.get()), source(sample, now));
    sample = m_topics.value("RobotPathPlanInfo");
    result.lastPathPlan = path(dynamic_cast<const wire::RobotPathPlanInfo *>(sample.message.get()), source(sample, now));
    sample = m_topics.value("Event");
    result.lastEvent = eventNotice(dynamic_cast<const wire::Event *>(sample.message.get()), source(sample, now));
    return result;
}

QList<StateNotification> StateStore::notifications() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto now = m_clock();
    QList<StateNotification> result;
    for (const auto &sample : m_history) {
        const auto s = source(sample, now);
        if (const auto *p = dynamic_cast<const wire::Event *>(sample.message.get())) result.append(eventNotice(p, s));
        else if (const auto *p = dynamic_cast<const wire::PenaltyInfo *>(sample.message.get())) result.append(penalty(p, s));
        else if (const auto *p = dynamic_cast<const wire::MapClickInfo *>(sample.message.get())) result.append(marker(p, s));
        else if (const auto *p = dynamic_cast<const wire::RobotPathPlanInfo *>(sample.message.get())) result.append(path(p, s));
        else if (const auto *p = dynamic_cast<const wire::SentryCtrlResult *>(sample.message.get())) result.append(sentryResult(p, s));
    }
    return result;
}

} // 命名空间 ly27
