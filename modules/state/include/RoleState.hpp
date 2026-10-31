#pragma once

#include "RobotState.hpp"
#include <variant>

// 角色可用的操作
namespace ly27 {

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

struct RuneState {
    StateField<quint32> status;
    StateField<quint32> activatedArms;
    StateField<float> averageRings;
};

struct SentryState {
    StateField<quint32> posture;
    StateField<bool> weakened;
    StateField<bool> powered;
};

struct DartState {
    StateField<quint32> targetId;
    StateField<quint32> openingState;
};

struct SentryControlResult {
    StateField<quint32> commandId;
    StateField<quint32> resultCode;
};

struct EventNotice {
    StateField<qint32> id;
    StateField<QString> parameter;
};

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

struct PathPlanState {
    StateField<quint32> intention;
    StateField<quint32> startXDecimeters;
    StateField<quint32> startYDecimeters;
    StateField<QList<qint32>> offsetsX;
    StateField<QList<qint32>> offsetsY;
    StateField<quint32> senderId;
};

// 三种角色使用同一批接收数据，角色只决定常用操作入口，不代表服务器授权。
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
