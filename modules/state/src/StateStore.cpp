#include "StateStore.hpp"
#include "rm2027.pb.h"

#include <QJsonArray>
#include <QThread>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

namespace ly27 {
namespace {
bool knownRobot(int id)
{
    return id == 1 || id == 3 || id == 4 || id == 7 ||
           id == 101 || id == 103 || id == 104 || id == 107;
}

std::optional<int> robotNumber(quint32 number)
{
    if (number > static_cast<quint32>(std::numeric_limits<int>::max())) return {};
    return static_cast<int>(number);
}

bool robotTopicName(const QString &topic)
{
    return topic == "RobotStaticStatus" || topic == "RobotDynamicStatus" ||
           topic == "RobotModuleStatus" || topic == "RobotInjuryStat" ||
           topic == "RobotRespawnStatus" || topic == "RobotPerformanceSelectionSync" ||
           topic == "RobotPosition" || topic == "PenaltyInfo" || topic == "CustomByteBlock";
}

bool historyTopic(const QString &topic)
{
    return topic == "Event" || topic == "PenaltyInfo" || topic == "MapClickInfo" ||
           topic == "RobotPathPlanInfo" || topic == "SentryCtrlResult";
}
} // 命名空间

QString stateValidityName(StateValidity validity)
{
    switch (validity) {
    case StateValidity::Unknown: return "unknown";
    case StateValidity::Fresh: return "fresh";
    case StateValidity::Stale: return "stale";
    case StateValidity::Disconnected: return "disconnected";
    case StateValidity::Retained: return "retained";
    case StateValidity::Observed: return "observed";
    }
    return "unknown";
}

QJsonObject StateValue::toJson() const
{
    return {{"fields", fields}, {"source", source}, {"validity", validity}, {"reason", reason},
            {"age_ms", ageMs ? QJsonValue(*ageMs) : QJsonValue(QJsonValue::Null)},
            {"ttl_ms", ttlMs ? QJsonValue(*ttlMs) : QJsonValue(QJsonValue::Null)}};
}

qint64 StateStore::monotonicNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

StateStore::StateStore(TerminalIdentity identity, QObject *parent, Clock clock)
    : QObject(parent), m_identity(std::move(identity)), m_clock(std::move(clock)), m_timer(this)
{
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &StateStore::refresh);
    reset({});
}

void StateStore::reset(const QString &sessionId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_timer.stop();
    m_sessionId = sessionId;
    m_sessionStartedNs = m_clock();
    m_roundStartedNs = m_sessionStartedNs;
    m_round.reset();
    m_roundRevision = 0;
    m_connected = false;
    m_connection = 0;
    clearSamples();
    m_validities = validities();
    emit changed({});
}

void StateStore::clearSamples()
{
    m_topics.clear();
    m_robots.clear();
    m_history.clear();
    for (const auto &name : MessageCodec::inboundTopics()) {
        Sample sample;
        sample.topic = name;
        m_topics.insert(name, sample);
    }
}

void StateStore::setConnected(bool connected)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_connected == connected) return;
    m_connected = connected;
    if (connected) {
        ++m_connection;
        m_timer.start();
    } else m_timer.stop();
    // 重新连上也不刷新旧样本的代次，必须等待新的消息。
    m_validities = validities();
    emit changed({});
}

bool StateStore::apply(const DecodedMessage &message, qint64 utcMs, qint64 monotonicNs, bool retained)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto info = MessageCodec::topicInfo(message.topic);
    if (!m_connected || m_sessionId.isEmpty() || !message.isParsed() || !message.message ||
        info.value("direction") != "inbound" || info.value("schema_status") != "defined" ||
        QString::fromStdString(message.message->GetDescriptor()->name()) != info.value("message").toString() ||
        monotonicNs < m_roundStartedNs || monotonicNs > m_clock()) return false;
    const auto previous = m_topics.constFind(message.topic);
    if (previous != m_topics.cend() && previous->received && monotonicNs < previous->monotonicNs)
        return false;

    bool roundChanged = false;
    const auto *game = dynamic_cast<const wire::GameStatus *>(message.message.get());
    if (game && game->has_current_round() && !retained) {
        if (m_round && *m_round != game->current_round()) {
            // 明确收到不同局次时清理上一局数据，不用时间或阶段猜测换局。
            clearSamples();
            ++m_roundRevision;
            m_roundStartedNs = monotonicNs;
            roundChanged = true;
        }
        m_round = game->current_round();
    }

    Sample sample;
    sample.topic = message.topic;
    sample.fields = message.fields;
    sample.message = message.message;
    sample.round = retained ? std::nullopt : m_round;
    sample.roundRevision = m_roundRevision;
    // repeated 没有字段存在性；将空列表显式保存，空列表能够清除旧列表。
    const auto *descriptor = message.message->GetDescriptor();
    for (int i = 0; i < descriptor->field_count(); ++i) {
        const auto *field = descriptor->field(i);
        if (field->is_repeated() && !sample.fields.contains(QString::fromStdString(field->name())))
            sample.fields.insert(QString::fromStdString(field->name()), QJsonArray{});
    }
    sample.sessionId = m_sessionId;
    sample.utcMs = utcMs;
    sample.monotonicNs = monotonicNs;
    sample.connection = m_connection;
    sample.received = true;
    sample.retained = retained;
    const double hz = info.value("frequency_hz").toDouble();
    // 工程缺省策略：允许三个周期，最低 500 ms；不是官方超时保证。
    if (hz > 0) sample.ttlMs = std::max<qint64>(500, static_cast<qint64>(std::ceil(3000 / hz)));
    const auto *buffMessage = dynamic_cast<const wire::Buff *>(sample.message.get());
    if (buffMessage && buffMessage->has_buff_left_time())
        sample.ttlMs = static_cast<qint64>(buffMessage->buff_left_time()) * 1000;

    if (buffMessage) {
        if (buffMessage->has_robot_id()) sample.robotId = robotNumber(buffMessage->robot_id());
    } else if (const auto *stat = dynamic_cast<const wire::RobotStaticStatus *>(sample.message.get())) {
        sample.robotId = stat->has_robot_id() ? robotNumber(stat->robot_id()) : m_identity.robotId;
    } else if (const auto *position = dynamic_cast<const wire::RobotPosition *>(sample.message.get())) {
        sample.robotId = position->has_robot_id() ? robotNumber(position->robot_id()) : m_identity.robotId;
    } else if (robotTopicName(message.topic)) sample.robotId = m_identity.robotId;
    m_topics[message.topic] = sample;
    if (sample.robotId && knownRobot(*sample.robotId)) {
        if (message.topic == "Buff") {
            if (buffMessage->has_buff_type() && buffMessage->buff_type() >= 1 && buffMessage->buff_type() <= 7)
                m_robots[*sample.robotId]["Buff/" + QString::number(buffMessage->buff_type())] = sample;
        } else m_robots[*sample.robotId][message.topic] = sample;
    }

    if (message.topic == "GlobalUnitStatus") {
        // 官方顺序为己方 1/3/4/7，再对方 1/3/4/7，不能当作绝对红蓝顺序。
        for (auto &robot : m_robots) robot.remove("GlobalHealth");
        const auto *units = dynamic_cast<const wire::GlobalUnitStatus *>(sample.message.get());
        if (units && units->robot_health_size() == 8) {
            const int ally = m_identity.team == Team::Red ? 0 : 100;
            const int enemy = ally == 0 ? 100 : 0;
            const int ids[] = {ally + 1, ally + 3, ally + 4, ally + 7,
                               enemy + 1, enemy + 3, enemy + 4, enemy + 7};
            for (int i = 0; i < 8; ++i) {
                auto projected = sample;
                projected.robotId = ids[i];
                projected.projectedHealth = units->robot_health(i);
                projected.fields = {{"current_health", static_cast<qint64>(*projected.projectedHealth)}};
                m_robots[ids[i]]["GlobalHealth"] = projected;
            }
        }
    }
    if (historyTopic(message.topic) && !sample.fields.isEmpty()) {
        m_history.append(sample);
        if (m_history.size() > HistoryLimit) m_history.removeFirst();
    }
    m_validities = validities();
    emit changed(roundChanged ? QString{} : message.topic);
    return true;
}

StateSource StateStore::source(const Sample &sample, qint64 nowNs) const
{
    StateSource value;
    value.topic = sample.topic;
    if (!sample.received) return value;
    value.received = true;
    value.sessionId = sample.sessionId;
    value.receivedUtcMs = sample.utcMs;
    value.receivedMonotonicNs = sample.monotonicNs;
    value.connection = sample.connection;
    value.retained = sample.retained;
    value.robotId = sample.robotId;
    value.round = sample.round;
    value.roundRevision = sample.roundRevision;
    value.ageMs = std::max<qint64>(0, (nowNs - sample.monotonicNs) / 1000000);
    if (sample.ttlMs >= 0) value.ttlMs = sample.ttlMs;
    if (sample.fields.isEmpty()) value.reason = "missing_fields";
    else if (!m_connected) { value.validity = StateValidity::Disconnected; value.reason = "transport_disconnected"; }
    else if (sample.connection != m_connection) { value.validity = StateValidity::Stale; value.reason = "previous_connection"; }
    else if (sample.retained) { value.validity = StateValidity::Retained; value.reason = "source_time_unknown"; }
    else if (sample.ttlMs >= 0 && *value.ageMs >= sample.ttlMs) {
        value.validity = StateValidity::Stale; value.reason = "timeout";
    } else if (sample.ttlMs < 0) {
        value.validity = StateValidity::Observed; value.reason = "conditional_report";
    } else { value.validity = StateValidity::Fresh; value.reason.clear(); }
    return value;
}

StateValue StateStore::view(const Sample &sample, qint64 nowNs) const
{
    const auto metadata = source(sample, nowNs);
    StateValue value;
    value.fields = sample.fields;
    value.validity = stateValidityName(metadata.validity);
    value.reason = metadata.reason;
    value.ageMs = metadata.ageMs;
    value.ttlMs = metadata.ttlMs;
    value.source = {{"transport", "mqtt"}, {"topic", metadata.topic}};
    if (!metadata.received) return value;
    value.source.insert("session_id", metadata.sessionId);
    value.source.insert("received_utc_ms", metadata.receivedUtcMs);
    value.source.insert("received_monotonic_ns", QString::number(metadata.receivedMonotonicNs));
    value.source.insert("connection", QString::number(metadata.connection));
    value.source.insert("retained", metadata.retained);
    value.source.insert("robot_id", metadata.robotId ? QJsonValue(*metadata.robotId) : QJsonValue(QJsonValue::Null));
    value.source.insert("round", metadata.round ? QJsonValue(static_cast<qint64>(*metadata.round)) : QJsonValue(QJsonValue::Null));
    value.source.insert("round_revision", QString::number(metadata.roundRevision));
    return value;
}

StateValue StateStore::topic(const QString &topic) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return view(m_topics.value(topic), m_clock());
}

StateValue StateStore::robotTopic(int robotId, const QString &topic) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return view(m_robots.value(robotId).value(topic), m_clock());
}

StateStore::Sample StateStore::healthSample(int robotId, qint64 nowNs) const
{
    const auto robot = m_robots.value(robotId);
    auto dynamic = robot.value("RobotDynamicStatus");
    const auto *message = dynamic_cast<const wire::RobotDynamicStatus *>(dynamic.message.get());
    if (message && message->has_current_health()) {
        dynamic.projectedHealth = message->current_health();
        dynamic.fields = {{"current_health", static_cast<qint64>(*dynamic.projectedHealth)}};
    } else dynamic.fields = {};
    const auto global = robot.value("GlobalHealth");
    const auto dynamicView = view(dynamic, nowNs);
    const auto globalView = view(global, nowNs);
    // 优先使用新鲜的本机实时血量，再使用全局血量；都无效时只保留最近一次观测。
    if (dynamicView.isFresh()) return dynamic;
    if (globalView.isFresh()) return global;
    if (dynamic.projectedHealth && (!global.projectedHealth || dynamic.monotonicNs >= global.monotonicNs))
        return dynamic;
    return global;
}

StateValue StateStore::health(int robotId, qint64 nowNs) const
{
    return view(healthSample(robotId, nowNs), nowNs);
}

StateValue StateStore::robotHealth(int robotId) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return health(robotId, m_clock());
}

StateValue StateStore::buff(int robotId, int type) const
{
    return robotTopic(robotId, "Buff/" + QString::number(type));
}

QJsonObject StateStore::robotSnapshot(int robotId, qint64 nowNs) const
{
    QJsonObject topics, buffs;
    const auto robot = m_robots.value(robotId);
    for (auto it = robot.cbegin(); it != robot.cend(); ++it) {
        if (it.key().startsWith("Buff/")) buffs.insert(it.key().mid(5), view(it.value(), nowNs).toJson());
        else if (it.key() != "GlobalHealth") topics.insert(it.key(), view(it.value(), nowNs).toJson());
    }
    return {{"robot_id", robotId}, {"health", health(robotId, nowNs).toJson()},
            {"topics", topics}, {"buffs", buffs}};
}

QJsonObject StateStore::snapshot() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto now = m_clock();
    QJsonObject topics, robots, global, role;
    for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it)
        topics.insert(it.key(), view(it.value(), now).toJson());
    for (auto it = m_robots.cbegin(); it != m_robots.cend(); ++it)
        robots.insert(QString::number(it.key()), robotSnapshot(it.key(), now));
    const QMap<QString, QString> globals{{"game", "GameStatus"}, {"units", "GlobalUnitStatus"},
        {"logistics", "GlobalLogisticsStatus"}, {"mechanisms", "GlobalSpecialMechanism"},
        {"rune", "RuneStatusSync"}, {"sentry", "SentryStatusSync"}, {"dart", "DartSelectTargetStatusSync"}};
    for (auto it = globals.cbegin(); it != globals.cend(); ++it) global.insert(it.key(), topics.value(it.value()));
    role.insert("name", roleName(m_identity.role));
    if (m_identity.role == Role::Heavy) role.insert("assembly", topics.value("TechCoreMotionStateSync"));
    if (m_identity.role == Role::Infantry) role.insert("performance", topics.value("RobotPerformanceSelectionSync"));
    if (m_identity.role == Role::Commander) {
        role.insert("sentry", topics.value("SentryStatusSync"));
        role.insert("dart", topics.value("DartSelectTargetStatusSync"));
        role.insert("sentry_control_result", topics.value("SentryCtrlResult"));
    }
    QJsonArray history;
    for (const auto &sample : m_history) history.append(view(sample, now).toJson());
    return {{"session_id", m_sessionId}, {"connected", m_connected}, {"global", global},
            {"round", m_round ? QJsonValue(static_cast<qint64>(*m_round)) : QJsonValue(QJsonValue::Null)},
            {"round_revision", QString::number(m_roundRevision)},
            {"self", m_identity.robotId ? QJsonValue(robotSnapshot(*m_identity.robotId, now)) : QJsonValue(QJsonValue::Null)},
            {"robots", robots}, {"role", role}, {"topics", topics}, {"recent_notifications", history}};
}

QMap<QString, QString> StateStore::validities() const
{
    const auto now = m_clock();
    QMap<QString, QString> result;
    for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it) {
        const auto state = view(it.value(), now);
        result.insert(it.key(), state.validity + ":" + state.reason);
    }
    // Buff 按机器人和类型独立计时，不能仅监视最新一条 Buff 消息。
    for (auto robot = m_robots.cbegin(); robot != m_robots.cend(); ++robot) {
        for (auto item = robot->cbegin(); item != robot->cend(); ++item) {
            const auto state = view(item.value(), now);
            result.insert(QString::number(robot.key()) + "/" + item.key(), state.validity + ":" + state.reason);
        }
    }
    return result;
}

void StateStore::refresh()
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto current = validities();
    if (current == m_validities) return;
    m_validities = current;
    emit changed({});
}

} // 命名空间 ly27
