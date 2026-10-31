#pragma once

#include "StateTypes.hpp"
#include "TerminalIdentity.hpp"

// 定义比赛以及全局数据
namespace ly27 {

enum class GameStage : quint32 {
    NotStarted = 0, Preparation = 1, SelfCheck = 2, Countdown = 3, Battle = 4, Settlement = 5
};

// MatchState：保存当前局次，比分，比赛阶段，剩余时间等全局信息
struct MatchState {
    StateField<quint32> currentRound;
    StateField<quint32> totalRounds;
    StateField<quint32> redScore;
    StateField<quint32> blueScore;
    StateField<GameStage> stage;
    StateField<qint32> remainingSeconds;
    StateField<qint32> elapsedSeconds;
    StateField<bool> paused;
    StateField<quint32> result;
    StateField<quint32> endReason;
    StateField<bool> inBattle;
};

// 基地
struct BaseState {
    Team team = Team::Red;
    StateField<quint32> currentHealth;
    StateField<quint32> defenseState;
    // 2027 的 base_shield 表示阵地控制状态，并非护盾血量。
    StateField<quint32> controlState;
};

// 前哨站
struct OutpostState {
    StateField<quint32> currentHealth;
    StateField<quint32> controlState;
};

// 后勤
struct LogisticsState {
    StateField<quint32> remainingEconomy;
    StateField<quint64> totalEconomyObtained;
    StateField<quint32> techLevel;
    StateField<quint32> encryptionLevel;
};

// 特殊机制
struct SpecialMechanism {
    quint32 id = 0;
    qint32 remainingSeconds = 0;
};

// GlobalState：双方基地、R/B 前哨站、后勤、伤害统计、控制度、特殊机制等
struct GlobalState {
    BaseState allyBase;
    BaseState enemyBase;
    // R/B 是官方前哨站名称，保留其含义，不当作本地阵营归属。
    OutpostState rOutpost;
    OutpostState bOutpost;
    StateField<QList<quint32>> robotHealth;
    StateField<QList<qint32>> robotBullets;
    StateField<quint32> allyDamage;
    StateField<quint32> enemyDamage;
    StateField<quint32> allyControlPoints;
    StateField<quint32> enemyControlPoints;
    LogisticsState logistics;
    StateField<QList<SpecialMechanism>> mechanisms;
};

} // 命名空间 ly27
