#include "TacticalMapController.hpp"
#include "RobotMapModel.hpp"
#include "BuffPresentation.hpp"

#include <QSet>
#include <algorithm>
#include <cmath>
#include <utility>

namespace ly27 {
namespace {
bool ground(int id)
{
    return id == 1 || id == 3 || id == 4 || id == 7 || id == 101 || id == 103 || id == 104 || id == 107;
}
bool ally(int id, Team team) { return (id >= 100) == (team == Team::Blue); }
const QVariantList &facilityData()
{
    // ID 取通信协议附录二；中心沿现有底图标定，与血条、点击和标记共用。
    static const QVariantList values{
        QVariantMap{{"id", 10}, {"key", "RedBase"}, {"field", "redBaseHp"}, {"blue", false},
                    {"outpost", false}, {"px", 171}, {"py", 461}, {"radius", 62}, {"maximum", 10000}},
        QVariantMap{{"id", 110}, {"key", "BlueBase"}, {"field", "blueBaseHp"}, {"blue", true},
                    {"outpost", false}, {"px", 1514}, {"py", 464}, {"radius", 62}, {"maximum", 10000}},
        QVariantMap{{"id", 11}, {"key", "ROutpost"}, {"field", "rOutpostHp"}, {"blue", false},
                    {"outpost", true}, {"px", 667}, {"py", 685}, {"radius", 30}, {"maximum", 1000}},
        QVariantMap{{"id", 111}, {"key", "BOutpost"}, {"field", "bOutpostHp"}, {"blue", true},
                    {"outpost", true}, {"px", 1028}, {"py", 237}, {"radius", 30}, {"maximum", 1000}}
    };
    return values;
}
QVariantMap facility(int id)
{
    for (const auto &entry : facilityData()) if (entry.toMap().value("id").toInt() == id) return entry.toMap();
    return {};
}
}

TacticalMapController::TacticalMapController(const StateStore &store, TerminalIdentity identity,
                                           QObject *parent, StateStore::Clock clock)
    : QObject(parent), m_store(store), m_identity(std::move(identity)), m_clock(std::move(clock)),
      m_blueView(m_identity.team == Team::Blue)
{
    connect(&store, &StateStore::changed, this, [this](const QString &topic) {
        refresh();
        // 只响应本次新通知，不在刷新、创建窗口或重连时重播历史消息。
        if (topic == "MapClickInfo") { observeMarker(); refresh(); }
    });
    connect(&m_timer, &QTimer::timeout, this, &TacticalMapController::refresh);
    m_timer.start(100);
    refresh();
}

QVariantList TacticalMapController::facilities() const { return facilityData(); }

void TacticalMapController::setCommandDispatcher(CommandDispatcher *commands)
{
    disconnect(m_commandObservation);
    m_commands = commands;
    if (!commands) return;
    m_commandObservation = connect(commands, &CommandDispatcher::observed, this, [this](const QJsonObject &event) {
        if (event.value("id").toString() != m_submission) return;
        const QString transport = event.value("transport").toString();
        if (transport == "failed" || transport == "uncertain")
            feedback(QStringLiteral("标记传输未确认：%1").arg(event.value("error").toString()));
    });
}

void TacticalMapController::feedback(const QString &text)
{
    if (m_feedback == text) return;
    m_feedback = text; emit feedbackChanged();
}

QPointF TacticalMapController::unprojectPosition(QPointF point, bool blueView)
{
    if (blueView) point = QPointF(1 - point.x(), 1 - point.y());
    return {std::clamp((point.x() * 1662 - 24) / 1614 * 28, 0.0, 28.0),
            std::clamp((1 - (point.y() * 946 - 24) / 897) * 15, 0.0, 15.0)};
}

void TacticalMapController::setBlueView(bool blue)
{
    if (m_blueView == blue) return;
    m_blueView = blue; updatePending(); refresh();
}

std::optional<QPointF> TacticalMapController::targetPosition(const Target &target) const
{
    if (!target.id) return RobotMapModel::projectPosition(target.position.x(), target.position.y(), m_blueView);
    const auto building = facility(target.id);
    if (!building.isEmpty()) {
        QPointF point(building.value("px").toDouble() / 1662, building.value("py").toDouble() / 946);
        return m_blueView ? QPointF(1 - point.x(), 1 - point.y()) : point;
    }
    const auto robot = m_store.robot(target.id);
    if (!robot || !robot->position.x.isFresh() || !robot->position.y.isFresh()) return {};
    return RobotMapModel::projectPosition(*robot->position.x.value, *robot->position.y.value, m_blueView);
}

void TacticalMapController::clickRobot(int id)
{
    if (!commander() || !ground(id) || !targetPosition({id, {}})) return;
    if (!m_selected) {
        if (!ally(id, m_identity.team)) { feedback(QStringLiteral("请先选择一台己方车接收指令")); return; }
        m_selected = id; emit selectionChanged();
        feedback(QStringLiteral("已选 %1 号车，请点击目标；右键或 Esc 取消").arg(id % 100));
        return;
    }
    if (ally(id, m_identity.team)) { beginTarget({id, {}}, {2}); chooseMarker(2); }
    else beginTarget({id, {}}, {1, 3});
}

void TacticalMapController::clickFacility(int id)
{
    if (!commander() || facility(id).isEmpty()) return;
    if (!m_selected) { feedback(QStringLiteral("请先选择一台己方车接收指令")); return; }
    const int type = ally(id, m_identity.team) ? 2 : 1;
    beginTarget({id, {}}, {type}); chooseMarker(type);
}

void TacticalMapController::clickMap(qreal x, qreal y)
{
    if (!commander() || !std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x > 1 || y > 1) return;
    if (!m_selected) { feedback(QStringLiteral("请先选择一台己方车接收指令")); return; }
    beginTarget({0, unprojectPosition({x, y}, m_blueView)}, {1, 2, 3});
}

void TacticalMapController::beginTarget(const Target &target, const QList<int> &options)
{
    m_pending = target; m_options = options; updatePending();
}

void TacticalMapController::updatePending()
{
    QVariantMap view;
    if (m_pending && m_options.size() > 1) {
        if (const auto point = targetPosition(*m_pending)) {
            QVariantList options;
            for (int type : m_options) options.append(type);
            view = {{"mapX", point->x()}, {"mapY", point->y()}, {"options", options}};
        }
    }
    if (m_pendingView != view) { m_pendingView = view; emit pendingChanged(); }
}

bool TacticalMapController::chooseMarker(int type)
{
    if (!commander() || !m_selected || !m_pending || !m_options.contains(type)) return false;
    if (!targetPosition(*m_pending)) { feedback(QStringLiteral("目标位置已失效，请重新选择")); return false; }
    if (!m_commands) { feedback(QStringLiteral("尚未连接指令接口")); return false; }
    MapRequest request;
    request.range = MapSendRange::ExceptSentry;
    request.receivers = {m_selected};
    request.marker = static_cast<MapMarker>(type);
    if (m_pending->id) request.target = MapEntity{m_pending->id};
    else request.target = MapPosition{float(m_pending->position.x()), float(m_pending->position.y())};
    const auto submission = m_commands->map(request);
    if (!submission.accepted) { feedback(QStringLiteral("未发送：%1").arg(submission.error)); return false; }
    m_submission = submission.id;
    cancel();
    feedback(QStringLiteral("标记已提交，等待服务器同步"));
    return true;
}

void TacticalMapController::cancel()
{
    if (m_selected) { m_selected = 0; emit selectionChanged(); }
    m_pending.reset(); m_options.clear(); updatePending(); feedback({});
}

void TacticalMapController::clearSession()
{
    m_active.clear(); m_submission.clear(); cancel();
}

void TacticalMapController::observeMarker()
{
    const auto mark = m_store.roleState().lastMapMarker;
    const auto &source = mark.markerType.source;
    if (source.validity != StateValidity::Observed || source.retained || !source.ageMs ||
        *source.ageMs >= MarkerLifetimeMs || !mark.sendRange.value || !mark.receiverIds.value ||
        !mark.markerType.value || !mark.markerMode.value || *mark.sendRange.value > 2 ||
        *mark.markerType.value < 1 || *mark.markerType.value > 3 || mark.receiverIds.value->size() != 7) return;
    QList<int> receivers;
    QSet<int> unique;
    bool padding = false;
    for (unsigned char id : *mark.receiverIds.value) {
        if (!id) { padding = true; continue; }
        if (padding || !ground(id) || !ally(id, m_identity.team) || unique.contains(id)) return;
        receivers.append(id); unique.insert(id);
    }
    // 本地约定：显式列表优先筛选；全零列表兼容协议广播消息。
    if (receivers.isEmpty()) {
        if (*mark.sendRange.value == 0) return;
        const int base = m_identity.team == Team::Red ? 0 : 100;
        receivers = {base + 1, base + 3, base + 4};
        if (*mark.sendRange.value == 2) receivers.append(base + 7);
    }
    if (!commander() && (!m_identity.robotId || !receivers.contains(*m_identity.robotId))) return;
    Target target;
    if (*mark.markerMode.value == 1) {
        if (!mark.x.value || !mark.y.value || !std::isfinite(*mark.x.value) || !std::isfinite(*mark.y.value) ||
            *mark.x.value < 0 || *mark.x.value > 28 || *mark.y.value < 0 || *mark.y.value > 15 ||
            (mark.enemyId.value && *mark.enemyId.value != 0)) return;
        target.position = {*mark.x.value, *mark.y.value};
    } else if (*mark.markerMode.value == 2) {
        if (!mark.enemyId.value || *mark.enemyId.value > 111) return;
        target.id = int(*mark.enemyId.value);
        if (!ground(target.id) && facility(target.id).isEmpty()) return;
        if (ally(target.id, m_identity.team) && *mark.markerType.value != 2) return;
    } else return;
    for (int receiver : receivers) {
        if (commander() || receiver == m_identity.robotId)
            m_active[receiver] = {target, int(*mark.markerType.value),
                m_clock() + (MarkerLifetimeMs - *source.ageMs) * 1000000};
    }
    feedback(QStringLiteral("已同步地图标记（显示 10 秒）"));
}

void TacticalMapController::refresh()
{
    // 即使还没有收到第一条标记，断线／换局清除位置后也不能遗留旧的待发选择。
    if (m_selected && !targetPosition({m_selected, {}})) cancel();
    const auto source = m_store.roleState().lastMapMarker.markerType.source;
    const QString epoch = source.received
        ? QStringLiteral("%1/%2/%3").arg(source.sessionId).arg(source.connection).arg(source.roundRevision) : QString{};
    if (epoch != m_epoch || !source.received || source.validity == StateValidity::Disconnected || source.reason == "previous_connection") {
        // 尚无通知时也允许选择目标；只在已建立的通知会话失效时清理。
        if (!m_epoch.isEmpty() || !m_active.isEmpty()) clearSession();
        m_epoch = epoch;
    }
    QVariantList views;
    for (auto it = m_active.begin(); it != m_active.end();) {
        if (m_clock() >= it->expiresNs) { it = m_active.erase(it); continue; }
        if (const auto point = targetPosition(it->target)) {
            const auto building = facility(it->target.id);
            views.append(QVariantMap{{"receiverId", it.key()}, {"targetId", it->target.id}, {"type", it->type},
                {"mapX", point->x()}, {"mapY", point->y()},
                {"kind", it->target.id == 0 ? "map" : building.isEmpty() ? "robot" : "facility"},
                {"buffRow", it->target.id != 0 && !mapBuffs(m_store.buffs(it->target.id)).isEmpty()},
                {"radius", building.value("radius", 0)}});
        }
        ++it;
    }
    if (m_markerViews != views) {
        m_markerViews = views;
        m_markerStates.clear();
        for (const auto &entry : views) {
            const auto value = entry.toMap();
            m_markerStates.insert(QString::number(value.value("receiverId").toInt()), value);
        }
        emit markersChanged();
    }
    updatePending();
}

}
