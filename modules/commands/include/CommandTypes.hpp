#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>
#include <variant>


// 定义“能够提出哪些操作请求”
namespace ly27 {

// 枚举类定义
enum class AssemblyOperation : quint32 { Start = 0, Confirm = 1, Cancel = 2 };
enum class ShooterPerformance : quint32 { Cooling = 1, Burst = 2 };
enum class ChassisPerformance : quint32 { Health = 1, Power = 2 };
enum class SentryControlMode : quint32 { Automatic = 0, SemiAutomatic = 1 };
enum class CommonAction : quint32 {
    Exchange17Ammo = 1, Exchange42Ammo = 2, ConfirmRespawn = 3,
    BuyRespawn = 4, RemoteAmmo = 5, RemoteHeal = 6
};
enum class SentryAction : quint32 {
    SupplyAmmo = 1, RemoteAmmo = 3, RemoteHeal = 4, ConfirmRespawn = 5, BuyRespawn = 6,
    Attack = 7, Defense = 8, Move = 9, EnhancedAttack = 10, EnhancedDefense = 11, EnhancedMove = 12
};
enum class DartTarget : quint32 {
    Outpost = 1, BaseFixed = 2, BaseRandomFixed = 3, BaseRandomMoving = 4, BaseEndMoving = 5
};
enum class MapSendRange : quint32 { Specified = 0, ExceptSentry = 1, IncludingSentry = 2 };
enum class MapMarker : quint32 { Attack = 1, Defense = 2, Warning = 3, Custom = 4 };

// Request：请求结构定义
struct AssemblyRequest { AssemblyOperation operation = AssemblyOperation::Start; quint32 difficulty = 1; };
struct PerformanceRequest {
    std::optional<ShooterPerformance> shooter;
    std::optional<ChassisPerformance> chassis;
};
struct SentryModeRequest { SentryControlMode mode = SentryControlMode::Automatic; };
struct CommonRequest {
    CommonAction action = CommonAction::ConfirmRespawn;
    std::optional<quint32> amount;
};
struct RuneRequest {};
struct DartRequest {
    std::optional<DartTarget> target;
    std::optional<bool> open;
    std::optional<bool> launch;
};
struct SentryRequest { SentryAction action = SentryAction::Move; };
struct MapPosition { float x = 0; float y = 0; };
struct MapEnemy { int robotId = 0; };
struct MapRequest {
    MapSendRange range = MapSendRange::Specified;
    QList<int> receivers;
    MapMarker marker = MapMarker::Attack;
    std::optional<char> ascii;
    std::optional<std::variant<MapPosition, MapEnemy>> target;
};

using CommandRequest = std::variant<AssemblyRequest, PerformanceRequest, SentryModeRequest,CommonRequest, RuneRequest, DartRequest, SentryRequest, MapRequest>;

// Submission：返回结果
struct CommandSubmission {
    bool accepted = false;
    QString id;
    QString code;
    QString error;
    qint64 retryAfterMs = 0;
    QJsonObject toJson() const;
};

struct CommandParseResult {
    std::optional<CommandRequest> request;
    QString error;
};

// JSON 只用于 CLI 边界；界面和其他 C++ 模块直接使用上面的请求结构
CommandParseResult parseCommandRequest(const QJsonObject &object);

} // 命名空间 ly27
