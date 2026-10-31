#pragma once

#include "StateTypes.hpp"
#include "TerminalIdentity.hpp"
#include <QByteArray>
#include <QMap>

// 定义一台机器人的所有数据
namespace ly27 {

enum class RobotLifeState : quint32 { Unknown = 0, Alive = 1, Dead = 2 };
enum class RobotModuleState : quint32 { Offline = 0, Online = 1, InstallationFault = 2 };
enum class BuffType : quint32 {
    Attack = 1, Defense = 2, Cooling = 3, ChassisPower = 4,
    Recovery = 5, RemoteAmmo = 6, TerrainWarning = 7
};

// 位置&朝向
struct PositionState {
    StateField<float> x;
    StateField<float> y;
    StateField<float> z;
    StateField<float> yawDegrees;
    StateField<quint32> reportedRobotId;
};

// 各模块状态
struct RobotModules {
    StateField<RobotModuleState> powerManager;
    StateField<RobotModuleState> rfid;
    StateField<RobotModuleState> lightStrip;
    StateField<RobotModuleState> smallShooter;
    StateField<RobotModuleState> bigShooter;
    StateField<RobotModuleState> uwb;
    StateField<RobotModuleState> armor;
    StateField<RobotModuleState> videoTransmission;
    StateField<RobotModuleState> capacitor;
    StateField<RobotModuleState> mainController;
    // 此字段是重装的安装权限，不是模块在线状态。
    StateField<quint32> modular42mmPermission;
};

// 复活状态
struct RespawnState {
    StateField<bool> pending;
    StateField<quint32> totalProgress;
    StateField<quint32> currentProgress;
    StateField<bool> canFreeRespawn;
    StateField<quint32> goldCost;
    StateField<bool> canPay;
    DerivedState<double> progressRatio;
};

// 受伤统计
struct InjuryState {
    StateField<quint32> totalDamage;
    StateField<quint32> collisionDamage;
    StateField<quint32> smallProjectileDamage;
    StateField<quint32> largeProjectileDamage;
    StateField<quint32> dartSplashDamage;
    StateField<quint32> moduleOfflineDamage;
    StateField<quint32> offlineDamage;
    StateField<quint32> penaltyDamage;
    StateField<quint32> serverKillDamage;
    StateField<quint32> killerId;
};

// 判罚信息
struct PenaltyState {
    StateField<quint32> type;
    StateField<quint32> effectSeconds;
    StateField<quint32> totalCount;
};

// 性能体系
struct PerformanceState {
    StateField<quint32> shooter;
    StateField<quint32> chassis;
    StateField<quint32> sentryControl;
};

// Buff
struct BuffState {
    StateField<quint32> robotId;
    StateField<BuffType> type;
    StateField<qint32> level;
    StateField<quint32> maximumSeconds;
    StateField<quint32> reportedRemainingSeconds;
};

// 机器人属性、实时数据和各子状态在这里汇总
struct RobotState {
    int robotId = 0;
    Team team = Team::Red;
    StateField<quint32> reportedRobotId;
    StateField<quint32> robotType;
    StateField<quint32> connectionState;
    StateField<quint32> fieldState;
    StateField<RobotLifeState> lifeState;
    StateField<bool> alive;
    StateField<quint32> level;
    StateField<quint32> currentHealth;
    StateField<quint32> maxHealth;
    StateField<float> currentHeat;
    StateField<quint32> maxHeat;
    StateField<float> heatCooldownPerSecond;
    StateField<float> lastProjectileSpeed;
    StateField<quint32> maxPower;
    StateField<quint32> currentBufferEnergy;
    StateField<quint32> maxBufferEnergy;
    StateField<quint32> maxChassisEnergy;
    StateField<quint32> currentExperience;
    StateField<quint32> experienceForUpgrade;
    StateField<quint32> totalProjectilesFired;
    StateField<quint32> remainingAmmo;
    StateField<bool> outOfCombat;
    StateField<quint32> outOfCombatCountdown;
    StateField<bool> canRemoteHeal;
    StateField<bool> canRemoteAmmo;
    DerivedState<double> healthRatio;
    DerivedState<double> heatRatio;
    RobotModules modules;
    PositionState position;
    RespawnState respawn;
    InjuryState injury;
    PenaltyState penalty;
    PerformanceState performance;
    QMap<BuffType, BuffState> buffs;
    // 机器人端自定义协议待后续另行约定
    StateField<QByteArray> customData;
};

} // 命名空间 ly27
