#pragma once

#include "RobotState.hpp"
#include <variant>

// 角色专项功能状态
namespace ly27 {

// 重装
struct AssemblyState {
    StateField<quint32> maximumDifficulty;
    StateField<quint32> basicState;
    StateField<quint32> putInState;
    StateField<quint32> moveState;
    StateField<quint32> rotateState;
    StateField<quint32> enemyCoreStatus;
    StateField<quint32> remainingTotalSeconds;
    StateField<quint32> remainingStepSeconds;
};

// 能量机关
struct RuneState {
    StateField<quint32> status;
    StateField<quint32> activatedArms;
    StateField<float> averageRings;
};

// 烧饼姿态
struct SentryState {
    StateField<quint32> posture;
    StateField<bool> weakened;
    StateField<bool> powered;
};

// 镖
struct DartState {
    StateField<quint32> targetId;
    StateField<quint32> openingState;
};

// 烧饼控制指令的反馈
struct SentryControlResult {
    StateField<quint32> commandId;
    StateField<quint32> resultCode;
};

// 事件
struct EventNotice {
    StateField<qint32> id;
    StateField<QString> parameter;
};

// 接受地图事件
struct MapMarkerState {
    StateField<quint32> sendRange;
    StateField<QByteArray> receiverIds;
    StateField<quint32> markerType;
    StateField<quint32> enemyId;
    StateField<quint32> ascii;
    StateField<quint32> markerMode;
    StateField<float> x;
    StateField<float> y;
};

// 烧饼规划路径
struct PathPlanState {
    StateField<quint32> intention;
    StateField<quint32> startXDecimeters;
    StateField<quint32> startYDecimeters;
    StateField<QList<qint32>> offsetsX;
    StateField<QList<qint32>> offsetsY;
    StateField<quint32> senderId;
};

// 三种角色使用同一批接收数据
struct RoleState {
    Role role = Role::Infantry;
    AssemblyState assembly;
    PerformanceState performance;
    RuneState rune;
    SentryState sentry;
    DartState dart;
    SentryControlResult lastSentryResult;
    MapMarkerState lastMapMarker;
    PathPlanState lastPathPlan;
    EventNotice lastEvent;
};

// 每条通知保持实际类型
using StateNotification = std::variant<EventNotice, PenaltyState, MapMarkerState,PathPlanState, SentryControlResult>;

} // 命名空间 ly27
