#include "CommandDispatcher.hpp"
#include "rm2027.pb.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QThread>
#include <QUuid>
#include <google/protobuf/util/json_util.h>
#include <cmath>
#include <type_traits>
#include <utility>

namespace ly27 {
namespace {
bool groundRobot(int id) { return id == 1 || id == 3 || id == 4 || id == 7 || id == 101 || id == 103 || id == 104 || id == 107; }
bool ally(int id, Team team) { return groundRobot(id) && ((id >= 100) == (team == Team::Blue)); }
bool validSentry(quint32 id) { return id == 1 || (id >= 3 && id <= 12); }
QJsonObject fields(const google::protobuf::Message &message)
{
    google::protobuf::util::JsonPrintOptions options;
    options.preserve_proto_field_names = true;
    std::string json;
    const auto result = google::protobuf::util::MessageToJsonString(message, &json, options);
    return result.ok() ? QJsonDocument::fromJson(QByteArray::fromStdString(json)).object() : QJsonObject{};
}
} // 命名空间

QJsonObject CommandSubmission::toJson() const
{
    return {{"accepted", accepted}, {"id", id}, {"code", code}, {"error", error}, {"retry_after_ms", retryAfterMs}};
}

CommandDispatcher::CommandDispatcher(TerminalIdentity identity, const StateStore &state, MqttTransport &mqtt,
                                     QObject *parent, Clock clock)
    : QObject(parent), m_identity(std::move(identity)), m_state(state), m_mqtt(mqtt),
      m_clock(std::move(clock)), m_timer(this)
{
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &CommandDispatcher::expirePending);
    connect(&mqtt, &MqttTransport::publishDelivered, this, [this](int token) {
        const auto it = m_pending.find(token);
        if (it == m_pending.end()) return;
        const auto id = it->id; m_pending.erase(it);
        update(id, "delivered");
    });
    connect(&mqtt, &MqttTransport::publishFailed, this, [this](int token, const QString &error) {
        const auto it = m_pending.find(token);
        if (it == m_pending.end()) return;
        const auto id = it->id; m_pending.erase(it);
        update(id, "failed", error);
    });
    connect(&mqtt, &MqttTransport::stateChanged, this, [this](const QString &state) {
        if (state != "ready") interruptPending("连接中断，已提交指令的执行情况未知");
        else if (m_running) emit ready();
    });
}

void CommandDispatcher::start(const QString &sessionId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    stop();
    m_sessionId = sessionId;
    m_startedNs = m_clock();
    m_running = !sessionId.isEmpty();
    m_accepted = m_rejected = 0;
    m_records.clear(); m_lastSent.clear(); m_lastServerResult = {};
    if (m_running) m_timer.start();
    if (isReady()) emit ready();
}

void CommandDispatcher::reconfigureIdentity(const TerminalIdentity &identity)
{
    Q_ASSERT(QThread::currentThread() == thread());
    Q_ASSERT(!m_running);
    m_identity = identity;
    m_sessionId.clear();
    m_startedNs = 0;
    m_accepted = m_rejected = 0;
    m_records.clear();
    m_pending.clear();
    m_lastSent.clear();
    m_lastServerResult = {};
}

void CommandDispatcher::stop()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_running = false;
    m_timer.stop();
    interruptPending("后端已停止，已提交指令的执行情况未知");
}

CommandSubmission CommandDispatcher::reject(const QString &code, const QString &error, qint64 retryMs)
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_rejected;
    CommandSubmission result{false, {}, code, error, retryMs};
    emit observed({{"event", "command_rejected"}, {"session_id", m_sessionId}, {"result", result.toJson()}});
    return result;
}

CommandSubmission CommandDispatcher::submit(const QString &topic, const google::protobuf::Message &message, qint64 intervalNs)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_running) return reject("not_running", "指令模块尚未启动");
    if (m_mqtt.snapshot().value("state") != "ready") return reject("not_ready", "MQTT 未就绪，不缓存或自动补发操作");
    const auto now = m_clock();
    const auto last = m_lastSent.constFind(topic);
    if (last != m_lastSent.cend() && now - *last < intervalNs)
        return reject("rate_limited", "该 Topic 的发送间隔不足", (intervalNs - (now - *last) + 999999) / 1000000);
    if (m_pending.size() >= 32) return reject("busy", "等待传输确认的指令过多");
    QString error;
    const auto payload = MessageCodec::encode(topic, message, error);
    if (!error.isEmpty()) return reject("encoding_error", error);
    int token = 0;
    if (!m_mqtt.publish(topic, payload, 1, &token)) return reject("publish_failed", "通信层拒绝提交指令");
    m_lastSent.insert(topic, now);
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto match = m_state.matchState();
    QJsonObject record{{"event", "command_submitted"}, {"id", id}, {"session_id", m_sessionId},
        {"topic", topic}, {"token", token}, {"transport", "queued"}, {"execution", "not_confirmed"},
        {"submitted_utc_ms", QDateTime::currentMSecsSinceEpoch()}, {"submitted_monotonic_ns", QString::number(now)},
        {"fields", fields(message)}, {"payload_base64", QString::fromLatin1(payload.toBase64())},
        {"round", match.currentRound.source.round ? QJsonValue(static_cast<qint64>(*match.currentRound.source.round)) : QJsonValue(QJsonValue::Null)}};
    // 不丢弃仍在等待确认的记录，窗口优先移除已经结束传输等待的旧记录。
    if (m_records.size() >= HistoryLimit) {
        for (auto it = m_records.begin(); it != m_records.end(); ++it) {
            if (it->value("transport") != "queued") { m_records.erase(it); break; }
        }
    }
    m_records.append(record);
    m_pending.insert(token, {id, now});
    ++m_accepted;
    emit observed(record);
    return {true, id, "queued", {}, 0};
}

CommandSubmission CommandDispatcher::requireFlag(const StateField<bool> &flag, const QString &description)
{
    if (!flag.isFresh()) return reject("state_unavailable", description + "状态未知或过期");
    if (!*flag.value) return reject("state_denied", "服务器当前未允许" + description);
    return {true, {}, {}, {}, 0};
}

CommandSubmission CommandDispatcher::dispatch(const CommandRequest &request)
{
    Q_ASSERT(QThread::currentThread() == thread());
    return std::visit([this](const auto &value) -> CommandSubmission {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AssemblyRequest>) return assembly(value);
        else if constexpr (std::is_same_v<T, PerformanceRequest>) return selectPerformance(value);
        else if constexpr (std::is_same_v<T, SentryModeRequest>) return selectSentryMode(value.mode);
        else if constexpr (std::is_same_v<T, CommonRequest>) return common(value);
        else if constexpr (std::is_same_v<T, RuneRequest>) return activateRune();
        else if constexpr (std::is_same_v<T, DartRequest>) return dart(value);
        else if constexpr (std::is_same_v<T, SentryRequest>) return sentry(value.action);
        else return map(value);
    }, request);
}

CommandSubmission CommandDispatcher::assembly(const AssemblyRequest &request)
{
    if (m_identity.role != Role::Heavy) return reject("role_denied", "装配操作仅供重装席位使用");
    const auto op = static_cast<quint32>(request.operation);
    if (op > 2 || request.difficulty < 1 || request.difficulty > 4) return reject("invalid_argument", "装配操作或难度无效");
    if (request.operation == AssemblyOperation::Start) {
        const auto maximum = m_state.roleState().assembly.maximumDifficulty;
        if (!maximum.isFresh()) return reject("state_unavailable", "可选装配难度未知或过期");
        if (request.difficulty > *maximum.value) return reject("state_denied", "装配难度尚未解锁");
    }
    wire::AssemblyCommand message;
    message.set_operation(op); message.set_difficulty(request.difficulty);
    return submit("AssemblyCommand", message);
}

CommandSubmission CommandDispatcher::selectPerformance(const PerformanceRequest &request)
{
    if (m_identity.role == Role::Commander) return reject("role_denied", "指挥手不能选择地面机器人的性能体系");
    if ((!request.shooter && !request.chassis) ||
        (request.shooter && (static_cast<quint32>(*request.shooter) < 1 || static_cast<quint32>(*request.shooter) > 2)) ||
        (request.chassis && (static_cast<quint32>(*request.chassis) < 1 || static_cast<quint32>(*request.chassis) > 2)))
        return reject("invalid_argument", "需要至少一个有效的性能体系选项");
    wire::RobotPerformanceSelectionCommand message;
    if (request.shooter) message.set_shooter(static_cast<quint32>(*request.shooter));
    if (request.chassis) message.set_chassis(static_cast<quint32>(*request.chassis));
    return submit("RobotPerformanceSelectionCommand", message);
}

CommandSubmission CommandDispatcher::selectSentryMode(SentryControlMode mode)
{
    if (m_identity.role != Role::Commander) return reject("role_denied", "哨兵控制方式由指挥手选择");
    if (static_cast<quint32>(mode) > 1) return reject("invalid_argument", "哨兵控制方式无效");
    wire::RobotPerformanceSelectionCommand message;
    message.set_sentry_control(static_cast<quint32>(mode));
    return submit("RobotPerformanceSelectionCommand", message);
}

CommandSubmission CommandDispatcher::common(const CommonRequest &request)
{
    if (m_identity.role == Role::Commander) return reject("role_denied", "指挥手的哨兵操作应使用哨兵指令");
    const auto action = static_cast<quint32>(request.action);
    if (action < 1 || action > 6) return reject("invalid_argument", "常用指令类型无效");
    if (action <= 2) {
        if (!request.amount || *request.amount == 0 || (action == 1 && *request.amount % 10 != 0))
            return reject("invalid_argument", "兑换数量必须为正数，17mm 数量必须为 10 的倍数");
        if ((action == 1 && m_identity.role != Role::Infantry) || (action == 2 && m_identity.role != Role::Heavy))
            return reject("role_denied", "该弹种与本地机器人角色不匹配");
    } else if (request.amount) return reject("invalid_argument", "该操作没有已定义的数量参数");
    const auto own = m_state.self();
    if (action >= 3) {
        if (!own) return reject("state_unavailable", "没有本机机器人状态");
        StateField<bool> flag;
        if (action == 3) flag = own->respawn.canFreeRespawn;
        if (action == 4) flag = own->respawn.canPay;
        if (action == 5) flag = own->canRemoteAmmo;
        if (action == 6) flag = own->canRemoteHeal;
        const auto check = requireFlag(flag, "复活或远程兑换");
        if (!check.accepted) return check;
    }
    wire::CommonCommand message;
    message.set_cmd_type(action);
    // 其余命令的 param 语义未定义，保持缺失，不猜测其为固定值。
    if (request.amount) message.set_param(*request.amount);
    return submit("CommonCommand", message);
}

CommandSubmission CommandDispatcher::activateRune()
{
    if (m_identity.role != Role::Infantry) return reject("role_denied", "能量机关激活入口供步兵席位使用");
    const auto status = m_state.roleState().rune.status;
    if (!status.isFresh()) return reject("state_unavailable", "能量机关状态未知或过期");
    if (*status.value != 1) return reject("state_denied", "能量机关当前不处于未激活状态");
    wire::RuneActivateCommand message; message.set_activate(1);
    return submit("RuneActivateCommand", message);
}

CommandSubmission CommandDispatcher::dart(const DartRequest &request)
{
    if (m_identity.role != Role::Commander) return reject("role_denied", "飞镖操作仅供指挥手席位使用");
    if ((!request.target && !request.open && !request.launch) ||
        (request.target && (static_cast<quint32>(*request.target) < 1 || static_cast<quint32>(*request.target) > 5)) ||
        (request.launch.value_or(false) && request.open && !*request.open))
        return reject("invalid_argument", "飞镖目标或操作组合无效");
    const auto gate = m_state.roleState().dart.openingState;
    if (request.target || request.launch.value_or(false)) {
        if (!gate.isFresh()) return reject("state_unavailable", "飞镖闸门状态未知或过期");
        if (request.target && *gate.value != 0) return reject("state_denied", "闸门关闭时才能更改飞镖目标");
        if (request.launch.value_or(false) && *gate.value != 2) return reject("state_denied", "闸门完全开启后才能确认发射");
    }
    wire::DartCommand message;
    if (request.target) message.set_target_id(static_cast<quint32>(*request.target));
    if (request.open) message.set_open(*request.open);
    if (request.launch) message.set_launch_confirm(*request.launch);
    return submit("DartCommand", message);
}

CommandSubmission CommandDispatcher::sentry(SentryAction action)
{
    if (m_identity.role != Role::Commander) return reject("role_denied", "哨兵操作仅供指挥手席位使用");
    if (!validSentry(static_cast<quint32>(action))) return reject("invalid_argument", "哨兵指令编号无效");
    wire::SentryCtrlCommand message; message.set_command_id(static_cast<quint32>(action));
    return submit("SentryCtrlCommand", message);
}

CommandSubmission CommandDispatcher::map(const MapRequest &request)
{
    const auto range = static_cast<quint32>(request.range);
    const auto marker = static_cast<quint32>(request.marker);
    if (range > 2 || marker < 1 || marker > 4 || !request.target)
        return reject("invalid_argument", "地图发送范围、标记或目标缺失/无效");
    if ((range == 0 && request.receivers.isEmpty()) || (range != 0 && !request.receivers.isEmpty()) || request.receivers.size() > 7)
        return reject("invalid_argument", "指定发送必须填写接收者，广播不能附带接收者");
    QSet<int> receivers;
    QByteArray ids(7, '\0');
    for (int i = 0; i < request.receivers.size(); ++i) {
        const int id = request.receivers[i];
        if (!ally(id, m_identity.team) || receivers.contains(id)) return reject("invalid_argument", "地图接收者必须是己方且不能重复");
        receivers.insert(id); ids[i] = static_cast<char>(id);
    }
    if (m_identity.role != Role::Commander &&
        (range != 0 || !m_identity.robotId || request.receivers != QList<int>{*m_identity.robotId}))
        return reject("role_denied", "地面操作手的半自动地图操作只能发送给本机");
    if (marker == 4) {
        if (!request.ascii) return reject("invalid_argument", "自定义地图图标需要 ASCII 字母");
        const char c = *request.ascii;
        if (!((c >= 'C' && c <= 'L') || c == 'N' || c == 'O' || (c >= 'Q' && c <= 'Z')))
            return reject("invalid_argument", "自定义地图图标字母不在协议允许范围内");
    } else if (request.ascii) return reject("invalid_argument", "普通地图标记不接受自定义字母");
    wire::MapClickCmd message;
    message.set_is_send_all(range); message.set_robot_id(ids.toStdString());
    message.set_mode(marker);
    if (request.ascii) message.set_ascii(static_cast<unsigned char>(*request.ascii));
    if (const auto *position = std::get_if<MapPosition>(&*request.target)) {
        if (!std::isfinite(position->x) || !std::isfinite(position->y) || position->x < 0 || position->y < 0)
            return reject("invalid_argument", "地图坐标必须为非负有限数");
        message.set_type(1); message.set_map_x(position->x); message.set_map_y(position->y);
    } else {
        const int id = std::get<MapEnemy>(*request.target).robotId;
        if (!groundRobot(id) || ally(id, m_identity.team)) return reject("invalid_argument", "地图敌方目标编号无效");
        message.set_type(2); message.set_enemy_id(id); message.set_map_x(0); message.set_map_y(0);
    }
    return submit("MapClickCmd", message, m_identity.role == Role::Commander ? 500000000 : 3000000000LL);
}

void CommandDispatcher::update(const QString &id, const QString &transport, const QString &error)
{
    for (auto &record : m_records) {
        if (record.value("id") != id) continue;
        record.insert("transport", transport); record.insert("error", error);
        auto event = record; event.insert("event", "command_transport");
        emit observed(event);
        return;
    }
}

void CommandDispatcher::interruptPending(const QString &reason)
{
    const auto pending = m_pending;
    m_pending.clear();
    for (const auto &item : pending) update(item.id, "uncertain", reason);
}

void CommandDispatcher::expirePending()
{
    const auto now = m_clock();
    QList<QString> expired;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (now - it->submittedNs >= qint64(AckTimeoutMs) * 1000000) {
            expired.append(it->id); it = m_pending.erase(it);
        } else ++it;
    }
    for (const auto &id : expired) update(id, "uncertain", "等待 MQTT 传输确认超时，执行情况未知");
}

void CommandDispatcher::observeServerResult(const DecodedMessage &message, qint64 monotonicNs, bool retained)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_running || retained || monotonicNs < m_startedNs || message.topic != "SentryCtrlResult" || !message.isParsed()) return;
    const auto *result = dynamic_cast<const wire::SentryCtrlResult *>(message.message.get());
    if (!result || !result->has_command_id() || !result->has_result_code()) return;
    // command_id 是操作种类而非请求唯一 ID，不能将此结果强行关联到某一次点击。
    m_lastServerResult = {{"topic", message.topic}, {"fields", message.fields},
                         {"correlation", "unavailable_no_request_id"}, {"session_id", m_sessionId}};
    emit observed({{"event", "server_result_observed"}, {"session_id", m_sessionId}, {"result", m_lastServerResult}});
}

QJsonObject CommandDispatcher::snapshot() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QJsonArray records;
    for (const auto &record : m_records) records.append(record);
    return {{"state", !m_running ? "stopped" : m_mqtt.snapshot().value("state") == "ready" ? "ready" : "offline"},
            {"session_id", m_sessionId}, {"accepted", m_accepted}, {"rejected", m_rejected},
            {"pending", static_cast<qint64>(m_pending.size())}, {"records", records}, {"last_server_result", m_lastServerResult}};
}

bool CommandDispatcher::isReady() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return m_running && m_mqtt.snapshot().value("state") == "ready";
}

} // 命名空间 ly27
