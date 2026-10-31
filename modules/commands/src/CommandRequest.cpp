#include "CommandTypes.hpp"

#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <cmath>
#include <limits>

// cli使用
namespace ly27 {
namespace {
struct Reader {
    QJsonObject object;
    QString error;

    void fail(const QString &message) { if (error.isEmpty()) error = message; }
    void keys(const QStringList &allowed)
    {
        for (auto it = object.begin(); it != object.end(); ++it)
            if (!allowed.contains(it.key())) fail("未知指令字段：" + it.key());
    }
    std::optional<quint32> number(const QString &key, bool required = true)
    {
        if (!object.contains(key) && !required) return {};
        const auto value = object.value(key);
        const double n = value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(n) || n < 0 || n > std::numeric_limits<quint32>::max() || std::floor(n) != n) {
            fail(key + " 必须是 uint32 整数"); return {};
        }
        return static_cast<quint32>(n);
    }
    std::optional<bool> boolean(const QString &key)
    {
        if (!object.contains(key)) return {};
        if (!object.value(key).isBool()) { fail(key + " 必须是布尔值"); return {}; }
        return object.value(key).toBool();
    }
    template<typename T>
    std::optional<T> choice(const QString &key, const QMap<QString, T> &values, bool required = true)
    {
        if (!object.contains(key) && !required) return {};
        const auto value = object.value(key);
        if (!value.isString() || !values.contains(value.toString())) { fail(key + " 选项无效"); return {}; }
        return values.value(value.toString());
    }
};
} // 命名空间

CommandParseResult parseCommandRequest(const QJsonObject &object)
{
    Reader r{object, {}};
    std::optional<CommandRequest> request;
    const auto name = object.value("command").toString();
    if (name == "assembly") {
        r.keys({"command", "operation", "difficulty"});
        const auto operation = r.choice<AssemblyOperation>("operation", {{"start", AssemblyOperation::Start},
            {"confirm", AssemblyOperation::Confirm}, {"cancel", AssemblyOperation::Cancel}});
        const auto difficulty = r.number("difficulty");
        if (operation && difficulty) request = AssemblyRequest{*operation, *difficulty};
    } else if (name == "performance") {
        r.keys({"command", "shooter", "chassis"});
        PerformanceRequest p;
        p.shooter = r.choice<ShooterPerformance>("shooter", {{"cooling", ShooterPerformance::Cooling}, {"burst", ShooterPerformance::Burst}}, false);
        p.chassis = r.choice<ChassisPerformance>("chassis", {{"health", ChassisPerformance::Health}, {"power", ChassisPerformance::Power}}, false);
        if (!p.shooter && !p.chassis) r.fail("至少指定 shooter 或 chassis");
        request = p;
    } else if (name == "sentry-mode") {
        r.keys({"command", "mode"});
        const auto mode = r.choice<SentryControlMode>("mode", {{"automatic", SentryControlMode::Automatic}, {"semi-automatic", SentryControlMode::SemiAutomatic}});
        if (mode) request = SentryModeRequest{*mode};
    } else if (name == "common") {
        r.keys({"command", "action", "amount"});
        const auto action = r.choice<CommonAction>("action", {{"exchange-17", CommonAction::Exchange17Ammo},
            {"exchange-42", CommonAction::Exchange42Ammo}, {"confirm-respawn", CommonAction::ConfirmRespawn},
            {"buy-respawn", CommonAction::BuyRespawn}, {"remote-ammo", CommonAction::RemoteAmmo}, {"remote-heal", CommonAction::RemoteHeal}});
        const auto amount = r.number("amount", false);
        if (action) {
            const bool exchange = *action == CommonAction::Exchange17Ammo || *action == CommonAction::Exchange42Ammo;
            if (exchange && (!amount || *amount == 0 || (*action == CommonAction::Exchange17Ammo && *amount % 10 != 0)))
                r.fail("兑换数量必须为正数，17mm 数量必须为 10 的倍数");
            if (!exchange && amount) r.fail("该操作不接受 amount");
            request = CommonRequest{*action, amount};
        }
    } else if (name == "rune") {
        r.keys({"command"}); request = RuneRequest{};
    } else if (name == "dart") {
        r.keys({"command", "target", "open", "launch"});
        DartRequest p;
        const auto target = r.number("target", false);
        if (target) {
            if (*target < 1 || *target > 5) r.fail("飞镖 target 必须为 1–5");
            p.target = static_cast<DartTarget>(*target);
        }
        p.open = r.boolean("open"); p.launch = r.boolean("launch");
        if (!p.target && !p.open && !p.launch) r.fail("飞镖指令至少需要一个操作字段");
        request = p;
    } else if (name == "sentry") {
        r.keys({"command", "action"});
        const auto action = r.choice<SentryAction>("action", {{"supply-ammo", SentryAction::SupplyAmmo},
            {"remote-ammo", SentryAction::RemoteAmmo}, {"remote-heal", SentryAction::RemoteHeal},
            {"confirm-respawn", SentryAction::ConfirmRespawn}, {"buy-respawn", SentryAction::BuyRespawn},
            {"attack", SentryAction::Attack}, {"defense", SentryAction::Defense}, {"move", SentryAction::Move},
            {"enhanced-attack", SentryAction::EnhancedAttack}, {"enhanced-defense", SentryAction::EnhancedDefense}, {"enhanced-move", SentryAction::EnhancedMove}});
        if (action) request = SentryRequest{*action};
    } else if (name == "map") {
        r.keys({"command", "range", "receivers", "marker", "ascii", "position", "enemy"});
        MapRequest p;
        const auto range = r.choice<MapSendRange>("range", {{"specified", MapSendRange::Specified}, {"except-sentry", MapSendRange::ExceptSentry}, {"including-sentry", MapSendRange::IncludingSentry}});
        const auto marker = r.choice<MapMarker>("marker", {{"attack", MapMarker::Attack}, {"defense", MapMarker::Defense}, {"warning", MapMarker::Warning}, {"custom", MapMarker::Custom}});
        if (range) p.range = *range;
        if (marker) p.marker = *marker;
        if (object.contains("receivers")) {
            if (!object.value("receivers").isArray()) r.fail("receivers 必须是数组");
            QSet<int> seen;
            for (const auto &item : object.value("receivers").toArray()) {
                Reader id{{{"id", item}}, {}};
                const auto number = id.number("id");
                if (!number || *number > 255 || seen.contains(static_cast<int>(*number))) r.fail("接收者编号无效或重复");
                else { p.receivers.append(static_cast<int>(*number)); seen.insert(static_cast<int>(*number)); }
            }
        }
        if (object.contains("ascii")) {
            const auto value = object.value("ascii");
            if (!value.isString() || value.toString().size() != 1 || value.toString().front().unicode() > 127) r.fail("ascii 必须是单个 ASCII 字符");
            else p.ascii = value.toString().front().toLatin1();
        }
        if (object.contains("position") == object.contains("enemy")) r.fail("地图目标必须且只能指定 position 或 enemy");
        if (object.contains("position")) {
            if (!object.value("position").isObject()) r.fail("position 必须是对象");
            Reader position{object.value("position").toObject(), {}};
            position.keys({"x", "y"});
            const auto x = position.object.value("x"), y = position.object.value("y");
            const auto valid = [](const QJsonValue &v) { return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() >= 0 && v.toDouble() <= std::numeric_limits<float>::max(); };
            if (!valid(x) || !valid(y)) position.fail("地图 x/y 必须是非负有限数");
            if (!position.error.isEmpty()) r.fail(position.error);
            else p.target = MapPosition{static_cast<float>(x.toDouble()), static_cast<float>(y.toDouble())};
        } else if (const auto enemy = r.number("enemy", false)) {
            if (*enemy > 255) r.fail("enemy 编号无效");
            else p.target = MapEnemy{static_cast<int>(*enemy)};
        }
        request = p;
    } else r.fail("未知 command，支持 assembly/performance/sentry-mode/common/rune/dart/sentry/map");
    if (!r.error.isEmpty()) return {{}, r.error};
    return {request, request ? QString{} : QStringLiteral("指令字段不完整")};
}

} // 命名空间 ly27
