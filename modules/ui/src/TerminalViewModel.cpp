#include "UiTheme.hpp"
#include "TerminalViewModel.hpp"
#include "BuffPresentation.hpp"

#include <algorithm>
#include <utility>
#include <QSet>

namespace ly27 {
namespace {
template<class T> QVariant number(const StateField<T> &field)
{
    return field.isFresh() ? QVariant::fromValue(*field.value) : QVariant(-1);
}
QString stageName(const StateField<GameStage> &stage)
{
    if (!stage.isFresh()) return QStringLiteral("等待比赛数据");
    switch (*stage.value) {
    case GameStage::NotStarted: return QStringLiteral("比赛未开始");
    case GameStage::Preparation: return QStringLiteral("准备阶段");
    case GameStage::SelfCheck: return QStringLiteral("裁判系统自检");
    case GameStage::Countdown: return QStringLiteral("开局倒计时");
    case GameStage::Battle: return QStringLiteral("比赛进行中");
    case GameStage::Settlement: return QStringLiteral("比赛结算");
    }
    return QStringLiteral("未知阶段");
}
QString baseStatusText(const BaseState &base)
{
    if (base.currentHealth.isFresh() && *base.currentHealth.value == 0)
        return QStringLiteral("基地已摧毁");
    if (!base.defenseState.isFresh()) return QStringLiteral("状态未知");
    switch (*base.defenseState.value) {
    case 0: return {}; // 按当前显示需求，不展示 100% 防御增益。
    case 1: return QStringLiteral("护甲未展开");
    case 2: return QStringLiteral("护甲展开");
    }
    return QStringLiteral("未知状态");
}
QString endReasonText(const StateField<quint32> &reason)
{
    if (!reason.isFresh()) return QStringLiteral("等待结束原因");
    switch (*reason.value) {
    case 1: return QStringLiteral("基地被摧毁");
    case 2: return QStringLiteral("累计控制度判定");
    case 3: return QStringLiteral("全队总攻击伤害判定");
    case 4: return QStringLiteral("全队机器人剩余血量判定");
    case 5: return QStringLiteral("平局");
    case 6: return QStringLiteral("异常终止");
    case 7: return QStringLiteral("主裁判手动判定胜负");
    default: return QStringLiteral("未知结束原因（%1）").arg(*reason.value);
    }
}
}

TerminalViewModel::TerminalViewModel(const StateStore &store, TerminalIdentity identity,
                                   QString redTeam, QString blueTeam, QObject *parent)
    : QObject(parent), m_store(store), m_identity(std::move(identity)),
      m_redTeam(std::move(redTeam)), m_blueTeam(std::move(blueTeam)),
      m_blueView(m_identity.team == Team::Blue), m_robots(store, m_identity, this),
      m_events(store, m_identity.team, this), m_tactics(store, m_identity, this),
      m_notices(store, m_identity.team, this)
{
    connect(&store, &StateStore::changed, this, [this](const QString &topic) {
        refresh(); observeBattleEvent(topic);
    });
    m_battleTimer.setSingleShot(true);
    connect(&m_battleTimer, &QTimer::timeout, this, &TerminalViewModel::clearBattleNotice);
    connect(&m_timer, &QTimer::timeout, this, &TerminalViewModel::refresh);
    m_timer.start(100);
    refresh();
    observeBattleEvent({});
}

void TerminalViewModel::suppressRestoredResult()
{
    m_suppressRestoredResult = true;
    refresh();
}

void TerminalViewModel::setBlueView(bool value)
{
    if (m_blueView == value) return;
    m_blueView = value;
    emit blueViewChanged();
    m_robots.refresh(value);
    m_tactics.setBlueView(value);
}

void TerminalViewModel::clearBattleNotice()
{
    m_battleTimer.stop();
    if (m_battleNotice.isEmpty()) return;
    m_battleNotice.clear();
    emit battleNoticeChanged();
}

void TerminalViewModel::observeBattleEvent(const QString &topic)
{
    const auto game = m_store.matchState();
    const auto event = m_store.roleState().lastEvent;
    const auto &gs = game.stage.source;
    const auto &es = event.id.source;
    const auto &s = es.received && (!gs.received || es.connection > gs.connection) ? es : gs;
    const QString epoch = s.received
        ? QStringLiteral("%1/%2/%3").arg(s.sessionId).arg(s.connection).arg(s.roundRevision) : QString{};
    if (epoch != m_battleEpoch || s.validity == StateValidity::Disconnected || s.reason == "previous_connection") {
        clearBattleNotice();
        m_battleEpoch = epoch;
        m_seenRoundStart = false; m_sawKill = false; m_killStreaks.clear();
    }
    if (!game.stage.isFresh()) return;
    const int stage = int(*game.stage.value);
    if (stage >= 1 && stage <= 3) {
        // 模拟器可重新演示同一局号，明确准备/倒计时时也重置本地击杀统计。
        m_seenRoundStart = true; m_sawKill = false; m_killStreaks.clear();
    }
    if (stage == 4 && game.elapsedSeconds.isFresh() && *game.elapsedSeconds.value == 0) m_seenRoundStart = true;
    if (stage != 4 || (game.paused.isFresh() && *game.paused.value)) {
        clearBattleNotice(); return;
    }
    // 只播报本次收到的有效事件，不将恢复的历史记录或 retained 消息重新弹出。
    if (topic != "Event" || !event.id.value || !event.parameter.value ||
        es.validity != StateValidity::Observed || es.retained ||
        es.sessionId != gs.sessionId || es.connection != gs.connection || es.roundRevision != gs.roundRevision) return;
    QVariantMap notice;
    QString parameter = event.parameter.value->trimmed();
    parameter.replace(QStringLiteral("，"), QStringLiteral(","));
    if (*event.id.value == 1) {
        const auto parts = parameter.split(',');
        if (parts.size() != 2) return;
        bool victimOk = false, killerOk = false;
        const int victim = parts[0].trimmed().toInt(&victimOk);
        int killer = parts[1].trimmed().toInt(&killerOk);
        if (!victimOk || !killerOk || victim <= 0 || victim >= 200 || victim == 100 ||
            killer < 0 || killer >= 200 || killer == 100) return;
        const bool deathOnly = killer == 0 || killer == victim || killer % 100 == 10 ||
            (killer >= 100) == (victim >= 100);
        m_killStreaks.remove(victim);
        QString text = QStringLiteral("阵亡");
        int streak = 0;
        bool first = false;
        if (!deathOnly) {
            const auto previous = m_killStreaks.value(killer, {-1, 0});
            streak = previous.first >= 0 && es.receivedMonotonicNs >= previous.first &&
                es.receivedMonotonicNs - previous.first <= 10000000000LL ? previous.second + 1 : 1;
            m_killStreaks.insert(killer, {es.receivedMonotonicNs, streak});
            first = m_seenRoundStart && !m_sawKill;
            m_sawKill = true;
            const QStringList combos{QStringLiteral("双杀"), QStringLiteral("三杀"), QStringLiteral("四杀"), QStringLiteral("五杀")};
            text = streak >= 2 && streak <= 5 ? combos[streak - 2] : first ? QStringLiteral("第一滴血") : QStringLiteral("摧毁");
        } else killer = 0;
        notice = {{"kind", "kill"}, {"killer", killer}, {"victim", victim}, {"text", text},
                  {"red", (killer ? killer : victim) < 100}, {"streak", streak}, {"firstBlood", first}};
    } else if (*event.id.value == 2) {
        bool ok = false; const int victim = parameter.toInt(&ok);
        if (!ok || (victim != 11 && victim != 111)) return;
        notice = {{"kind", "outpost"}, {"killer", 0}, {"victim", victim},
                  {"text", QStringLiteral("被摧毁")}, {"red", victim == 11}};
    } else return;
    notice.insert("token", ++m_battleSequence);
    m_battleNotice = notice;
    // 与 26 相同，新播报即时替换旧播报；完整事件仍留在左侧历史栏。
    m_battleTimer.start(3500);
    emit battleNoticeChanged();
}

void TerminalViewModel::refresh()
{
    updateInjury();
    updatePlayerHud();
    m_notices.refresh();
    const auto selfBuffs = m_identity.role != Role::Commander && m_identity.robotId
        ? mapBuffs(m_store.buffs(*m_identity.robotId)) : QVariantList{};
    if (selfBuffs != m_selfBuffs) { m_selfBuffs = selfBuffs; emit selfBuffsChanged(); }
    QVariantMap facilityBuffs;
    for (int id : {10, 11, 110, 111}) facilityBuffs.insert(QString::number(id), mapBuffs(m_store.buffs(id)));
    if (m_facilityBuffs != facilityBuffs) { m_facilityBuffs = facilityBuffs; emit facilityBuffsChanged(); }
    const auto game = m_store.matchState();
    const auto global = m_store.globalState();
    const auto &outpostSource = global.rOutpost.controlState.source;
    const QString outpostEpoch = QStringLiteral("%1/%2/%3").arg(outpostSource.sessionId)
        .arg(outpostSource.connection).arg(outpostSource.roundRevision);
    if (m_outpostEpoch != outpostEpoch) {
        m_outpostEpoch = outpostEpoch;
        m_outpostOwners[0].clear(); m_outpostOwners[1].clear();
    }
    const bool redAlly = m_identity.team == Team::Red;
    const auto &redBase = redAlly ? global.allyBase : global.enemyBase;
    const auto &blueBase = redAlly ? global.enemyBase : global.allyBase;
    const bool paused = game.paused.isFresh() && *game.paused.value;
    QString timer = QStringLiteral("--:--");
    if (game.remainingSeconds.isFresh() && *game.remainingSeconds.value >= 0) {
        const int seconds = *game.remainingSeconds.value;
        timer = QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0'))
                                      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
    // 占领状态按通信身份解释，速度单位始终为点/比赛秒，不取累计量差分。
    QVariant controlNetRate;
    const auto &ownBase = global.allyBase.controlState;
    const auto &foeBase = global.enemyBase.controlState;
    const auto &r = global.rOutpost.controlState;
    const auto &b = global.bOutpost.controlState;
    if (game.stage.isFresh() && *game.stage.value == GameStage::Battle && game.paused.isFresh() &&
        ownBase.isFresh() && foeBase.isFresh() && r.isFresh() && b.isFresh() &&
        *ownBase.value >= 1 && *ownBase.value <= 2 && *foeBase.value >= 1 && *foeBase.value <= 2 &&
        *r.value >= 1 && *r.value <= 4 && *b.value >= 1 && *b.value <= 4) {
        const int allyRate = int(*r.value <= 2 && *b.value <= 2) +
            3 * int(*ownBase.value == 1 && *foeBase.value == 2);
        const int enemyRate = int(*r.value >= 3 && *b.value >= 3) +
            3 * int(*ownBase.value == 2 && *foeBase.value == 1);
        controlNetRate = paused ? 0 : allyRate - enemyRate;
    }
    QVariantMap values{
        {"redTeam", m_redTeam}, {"blueTeam", m_blueTeam}, {"timer", timer},
        {"currentRound", number(game.currentRound)}, {"totalRounds", number(game.totalRounds)},
        {"redScore", number(game.redScore)}, {"blueScore", number(game.blueScore)},
        {"stage", paused ? QStringLiteral("比赛暂停") : stageName(game.stage)},
        {"paused", paused}, {"fresh", game.remainingSeconds.isFresh()},
        // 只控制展示动效：失效及换会话时立即清掉离场快照，避免旧通告跨局残留。
        {"noticesLive", game.stage.isFresh() && *game.stage.value == GameStage::Battle && game.paused.isFresh() && !paused},
        {"noticeEpoch", game.stage.source.received ? QStringLiteral("%1/%2/%3")
            .arg(game.stage.source.sessionId).arg(game.stage.source.connection).arg(game.stage.source.roundRevision) : QString{}},
        {"redBaseHp", number(redBase.currentHealth)}, {"blueBaseHp", number(blueBase.currentHealth)},
        {"redBaseState", number(redBase.defenseState)}, {"blueBaseState", number(blueBase.defenseState)},
        {"redBaseStatusText", baseStatusText(redBase)}, {"blueBaseStatusText", baseStatusText(blueBase)},
        {"allyEconomy", number(global.logistics.remainingEconomy)},
        // 累计经济是 uint64，转为十进制文本以免 QML 数值转换丢失大整数精度。
        {"allyTotalEconomy", global.logistics.totalEconomyObtained.isFresh()
            ? QVariant(QString::number(*global.logistics.totalEconomyObtained.value)) : QVariant(-1)},
        {"allyTeamLabel", redAlly ? QStringLiteral("红方") : QStringLiteral("蓝方")},
        {"redControlPoints", number(redAlly ? global.allyControlPoints : global.enemyControlPoints)},
        {"blueControlPoints", number(redAlly ? global.enemyControlPoints : global.allyControlPoints)},
        {"redDamage", number(redAlly ? global.allyDamage : global.enemyDamage)},
        {"blueDamage", number(redAlly ? global.enemyDamage : global.allyDamage)},
        {"controlNetRate", controlNetRate},
        {"allyTechLevel", number(global.logistics.techLevel)},
        {"allyEncryptionLevel", number(global.logistics.encryptionLevel)},
        {"rOutpostHp", number(global.rOutpost.currentHealth)}, {"bOutpostHp", number(global.bOutpost.currentHealth)},
        {"rOutpostOwner", outpostOwner(global.rOutpost, 0)}, {"bOutpostOwner", outpostOwner(global.bOutpost, 1)}
    };
    const auto previousPhase = m_phase;
    const auto previousMechanisms = m_mechanisms;
    updatePresentation(game, global, paused);
    updateTerritories(global);
    const bool different = m_match != values || previousPhase != m_phase;
    m_match = values;
    if (previousMechanisms != m_mechanisms) emit mechanismsChanged();
    if (different) emit changed();
    m_robots.refresh(m_blueView);
}

QString TerminalViewModel::outpostOwner(const OutpostState &outpost, int index)
{
    if (!outpost.controlState.isFresh() || !outpost.currentHealth.isFresh()) return "unknown";
    const auto state = *outpost.controlState.value;
    if (state < 1 || state > 4) return "unknown";
    // 规则只允许通过击毁改变前哨阵地的完全控制方；临时占领不能给建筑换色。
    // 1 Hz 可能略过零血量，因此以完全控制方及已存活的同步快照确认当前拥有方，绝不补写 500 HP。
    if ((state == 1 || state == 4) && *outpost.currentHealth.value > 0) {
        const bool red = state == 1 ? m_identity.team == Team::Red : m_identity.team == Team::Blue;
        m_outpostOwners[index] = red ? "red" : "blue";
    }
    return m_outpostOwners[index].isEmpty() ? QStringLiteral("unknown") : m_outpostOwners[index];
}

void TerminalViewModel::updateTerritories(const GlobalState &global)
{
    const bool redAlly = m_identity.team == Team::Red;
    const auto state = [](bool known, bool red, bool temporary) {
        return QVariantMap{{"known", known}, {"controller", known ? (red ? "red" : "blue") : "unknown"},
            {"temporary", known && temporary}, {"color", known ? (red ? ui::TeamRed : ui::TeamBlue) : ui::TerritoryUnknown},
            {"advancing", ""}, {"seconds", -1}};
    };
    const auto outpost = [&](const StateField<quint32> &field) {
        if (!field.isFresh() || *field.value < 1 || *field.value > 4) return state(false, false, false);
        const auto value = *field.value;
        return state(true, value <= 2 ? redAlly : !redAlly, value == 2 || value == 3);
    };
    const auto base = [&](const StateField<quint32> &field, bool originalRed) {
        if (!field.isFresh() || *field.value < 1 || *field.value > 2) return state(false, false, false);
        // 基地枚举以该基地原始拥有方为准：1 原方完全控制，2 对方临时控制。
        return state(true, *field.value == 1 ? originalRed : !originalRed, *field.value == 2);
    };
    const auto &redBase = redAlly ? global.allyBase : global.enemyBase;
    const auto &blueBase = redAlly ? global.enemyBase : global.allyBase;
    const QStringList keys{"rOutpost", "bOutpost", "redBase", "blueBase"};
    QVariantMap values{{keys[0], outpost(global.rOutpost.controlState)}, {keys[1], outpost(global.bOutpost.controlState)},
        {keys[2], base(redBase.controlState, true)}, {keys[3], base(blueBase.controlState, false)}};
    for (const auto &notice : m_mechanisms) {
        const auto mechanism = notice.toMap();
        const int id = mechanism.value("id").toInt();
        if (id < 3 || id > 6) continue;
        auto region = values.value(keys[id - 3]).toMap();
        const bool red = mechanism.value("enemy").toBool() ? !redAlly : redAlly;
        region.insert("advancing", red ? "red" : "blue");
        region.insert("seconds", mechanism.value("seconds"));
        values.insert(keys[id - 3], region);
    }
    if (values != m_territories) { m_territories = values; emit territoriesChanged(); }
}

void TerminalViewModel::updateInjury()
{
    const auto robot = m_store.self();
    const InjuryState injury = robot ? robot->injury : InjuryState{};
    const auto valueText = [](const StateField<quint32> &field) {
        return field.isFresh() ? QString::number(*field.value) : QStringLiteral("—");
    };
    struct Row {
        const char *key;
        const char *label;
        const StateField<quint32> *field;
        const char *icon;
    };
    const Row definitions[] = {
        {"smallProjectile", "17mm 弹丸", &injury.smallProjectileDamage, "sundry_bullet.png"},
        {"largeProjectile", "42mm 弹丸", &injury.largeProjectileDamage, "sundry_golf.png"},
        {"collision", "撞击", &injury.collisionDamage, "sundry_icon.png"},
        {"dartSplash", "飞镖溅射", &injury.dartSplashDamage, "sundry_icon.png"},
        {"moduleOffline", "模块离线", &injury.moduleOfflineDamage, "sundry_icon.png"},
        {"offline", "异常离线", &injury.offlineDamage, "sundry_icon.png"},
        {"penalty", "判罚", &injury.penaltyDamage, "sundry_icon.png"},
        {"serverKill", "服务器强制战亡", &injury.serverKillDamage, "sundry_icon.png"}
    };
    QVariantList rows;
    for (const auto &row : definitions) {
        QString percent = QStringLiteral("—");
        // 缺字段不补零；仅总量和分项同时有效时计算占比，不合并不同伤害来源。
        if (row.field->isFresh() && injury.totalDamage.isFresh()) {
            if (*injury.totalDamage.value > 0)
                percent = QString::number(100.0 * *row.field->value / *injury.totalDamage.value, 'f', 1) + "%";
            else if (*row.field->value == 0) percent = QStringLiteral("0.0%");
        }
        rows.append(QVariantMap{{"key", row.key}, {"label", QString::fromUtf8(row.label)},
            {"valueText", valueText(*row.field)}, {"percentText", percent}, {"icon", row.icon}});
    }
    QString killer = QStringLiteral("未知");
    if (injury.killerId.isFresh()) {
        const auto id = *injury.killerId.value;
        if (id == 0) killer = QStringLiteral("未知／环境伤害");
        else if (id == 1 || id == 3 || id == 4 || id == 7 || id == 101 || id == 103 || id == 104 || id == 107)
            killer = QStringLiteral("%1 %2 号").arg(id >= 100 ? QStringLiteral("蓝方") : QStringLiteral("红方")).arg(id % 100);
        else killer = QStringLiteral("编号 %1").arg(id);
    }
    const RobotModules modules = robot ? robot->modules : RobotModules{};
    struct ModuleRow {
        const char *key;
        const char *label;
        const StateField<RobotModuleState> *field;
    };
    const ModuleRow moduleDefinitions[] = {
        {"armor", "装甲", &modules.armor},
        {"videoTransmission", "图传", &modules.videoTransmission},
        {"smallShooter", "17mm 测速", &modules.smallShooter},
        {"bigShooter", "42mm 测速", &modules.bigShooter},
        {"rfid", "RFID", &modules.rfid},
        {"uwb", "UWB 定位", &modules.uwb},
        {"lightStrip", "灯条", &modules.lightStrip},
        {"powerManager", "电源管理", &modules.powerManager},
        {"capacitor", "电容", &modules.capacitor},
        {"mainController", "主控", &modules.mainController}
    };
    QVariantList moduleRows;
    for (const auto &module : moduleDefinitions) {
        int state = -1;
        QString text = QStringLiteral("未知");
        if (module.field->isFresh()) {
            // 未识别枚举与缺失/过期同样显示未知，不把安装权限当成在线状态。
            switch (*module.field->value) {
            case RobotModuleState::Offline: state = 0; text = QStringLiteral("离线"); break;
            case RobotModuleState::Online: state = 1; text = QStringLiteral("在线"); break;
            case RobotModuleState::InstallationFault: state = 2; text = QStringLiteral("安装异常"); break;
            }
        }
        moduleRows.append(QVariantMap{{"key", module.key}, {"label", QString::fromUtf8(module.label)},
                                     {"state", state}, {"stateText", text}});
    }
    const QVariantMap values{{"rows", rows}, {"modules", moduleRows}, {"totalText", valueText(injury.totalDamage)}, {"killerText", killer},
        {"robotText", robot ? QStringLiteral("%1 %2 号").arg(robot->team == Team::Blue ? QStringLiteral("蓝方") : QStringLiteral("红方")).arg(robot->robotId % 100)
                            : QStringLiteral("当前身份未绑定机器人")}};
    if (m_injury != values) { m_injury = values; emit injuryChanged(); }
}

void TerminalViewModel::updatePresentation(const MatchState &game, const GlobalState &global, bool paused)
{
    const auto &s = game.stage.source;
    const QString epoch = s.received ? QStringLiteral("%1/%2/%3").arg(s.sessionId).arg(s.connection).arg(s.roundRevision) : QString{};
    if (epoch != m_phaseEpoch) {
        m_phaseEpoch = epoch; m_lastStage = -1; m_startUntilNs = 0;
        m_animateVictory = false;
    }
    m_phase = {{"kind", "none"}, {"token", epoch}};
    m_mechanisms.clear();
    if (!game.stage.isFresh() || static_cast<quint32>(*game.stage.value) > 5) {
        m_animateVictory = false;
        m_lastStage = -1; m_startUntilNs = 0; return;
    }
    const int stage = static_cast<int>(*game.stage.value);
    // 身份切换后的首包结算是状态恢复，不弹静态战报或胜利动画。
    // 只有该身份真正收到比赛进行中的新鲜状态，才允许后续正常结算展示。
    if (stage == 4) m_suppressRestoredResult = false;
    if (stage == 5 && m_suppressRestoredResult) return;
    const qint64 nowNs = s.receivedMonotonicNs + s.ageMs.value_or(0) * 1000000;
    if (stage == 4 && m_lastStage == 3) m_startUntilNs = nowNs + 1200000000LL;
    // 启动或重连后的结算快照不重播；胜者延迟上报时保留本次真实阶段切换的播放资格。
    if (stage != 5) m_animateVictory = false;
    else if (m_lastStage != 5) m_animateVictory = m_lastStage == 4;
    m_lastStage = stage;
    const int remaining = game.remainingSeconds.isFresh() && *game.remainingSeconds.value >= 0 ? *game.remainingSeconds.value : -1;
    m_phase.insert("token", epoch + "/" + QString::number(stage));
    m_phase.insert("remaining", remaining);
    m_phase.insert("timer", remaining >= 0
        ? QStringLiteral("%1:%2").arg(remaining / 60, 2, 10, QLatin1Char('0')).arg(remaining % 60, 2, 10, QLatin1Char('0'))
        : QStringLiteral("--:--"));
    if (stage == 5) {
        const int winner = game.result.isFresh() && *game.result.value <= 2 ? int(*game.result.value) : -1;
        const bool terminated = game.endReason.isFresh() && *game.endReason.value == 6;
        m_phase.insert("kind", winner >= 0 || terminated ? "result" : "settlement");
        m_phase.insert("title", winner == 1 ? QStringLiteral("红方胜利") : winner == 2 ? QStringLiteral("蓝方胜利")
            : winner == 0 ? QStringLiteral("平局") : terminated ? QStringLiteral("比赛异常终止") : QStringLiteral("等待裁判结算"));
        m_phase.insert("art", winner == 1 ? "red" : winner == 2 ? "blue" : "");
        m_phase.insert("animateVictory", m_animateVictory && (winner == 1 || winner == 2));
        m_phase.insert("description", endReasonText(game.endReason));
        m_phase.insert("token", epoch + "/result/" + QString::number(winner) + (terminated ? "/terminated" : ""));
    } else if (paused) {
        m_phase.insert("kind", "pause"); m_phase.insert("title", QStringLiteral("比赛暂停"));
        m_phase.insert("description", QStringLiteral("等待裁判系统恢复比赛"));
    } else if (stage == 1 || stage == 2) {
        m_phase.insert("kind", "preparation");
        m_phase.insert("title", stageName(game.stage));
        m_phase.insert("urgent", stage == 1 && remaining >= 0 && remaining <= 15);
        m_phase.insert("description", stage == 2 ? QStringLiteral("裁判系统自检，请勿抢跑") : QStringLiteral("请完成赛前设备与机器人检查"));
    } else if (stage == 3 && remaining >= 0 && remaining <= 5) {
        m_phase.insert("kind", "countdown");
    } else if (stage == 4 && nowNs < m_startUntilNs) {
        m_phase.insert("kind", "start");
    }
    if (stage != 4 || paused || !global.mechanisms.isFresh()) return;
    const QStringList regions{QStringLiteral("红方前哨阵地"), QStringLiteral("蓝方前哨阵地"),
                              QStringLiteral("红方基地阵地"), QStringLiteral("蓝方基地阵地")};
    QSet<quint32> seen;
    for (const auto &mechanism : *global.mechanisms.value) {
        const auto id = mechanism.id;
        const qint64 raw = mechanism.remainingSeconds;
        if (id < 1 || id > 6 || raw == 0 || (id <= 2 && raw < 0) || seen.contains(id)) continue;
        seen.insert(id);
        const bool enemy = id == 1 || (id >= 3 && raw < 0);
        const QString target = id == 1 ? QStringLiteral("己方堡垒") : id == 2 ? QStringLiteral("敌方堡垒") : regions[int(id) - 3];
        const QString actor = enemy ? QStringLiteral("敌方") : id >= 3 ? QStringLiteral("我方") : QStringLiteral("己方");
        const qint64 seconds = raw < 0 ? -raw : raw;
        const QString message = id >= 3 ? QStringLiteral("%1正在占领%2！").arg(actor, target)
            : QStringLiteral("%1还有 %2 秒").arg(actor).arg(seconds);
        const QString detail = id >= 3 ? QStringLiteral("倒计时%1秒").arg(seconds) : QStringLiteral("激活") + target;
        m_mechanisms.append(QVariantMap{{"id", id}, {"seconds", seconds}, {"target", target},
            {"actor", actor}, {"enemy", enemy}, {"accent", enemy ? ui::TeamRed : ui::Positive},
            {"message", message}, {"detail", detail}, {"text", message + "\n" + detail}});
    }
    std::stable_sort(m_mechanisms.begin(), m_mechanisms.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("seconds").toLongLong() < b.toMap().value("seconds").toLongLong();
    });
}

}
