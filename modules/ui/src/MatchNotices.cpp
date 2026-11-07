#include "UiTheme.hpp"
#include "MatchNotices.hpp"
#include "EventListModel.hpp"

#include <array>
#include <utility>

namespace ly27 {
namespace {
struct Node { int elapsed; const char *text; };
// 规则 §5.6.3、§5.7.3，时间为开局后的比赛秒数。
const std::array<Node, 8> nodes{{
    {0, "小能量机关"}, {30, "飞镖闸门"}, {90, "小能量机关"},
    {180, "大能量机关"}, {240, "飞镖闸门"}, {255, "大能量机关"},
    {330, "大能量机关"}, {420, "本局比赛即将结束"}
}};
bool sameEpoch(const StateSource &a, const StateSource &b)
{
    return a.sessionId == b.sessionId && a.connection == b.connection && a.roundRevision == b.roundRevision;
}
}
MatchNotices::MatchNotices(const StateStore &store, Team team, QObject *parent)
    : QObject(parent), m_store(store), m_team(team)
{
    m_eventTimer.setSingleShot(true);
    connect(&m_eventTimer, &QTimer::timeout, this, &MatchNotices::clearEvent);
    connect(&store, &StateStore::changed, this, &MatchNotices::observe);
    refresh();
}
void MatchNotices::setTimed(QVariantList value)
{
    if (m_timed == value) return;
    m_timed = std::move(value); emit timedChanged();
}
void MatchNotices::clearEvent()
{
    m_eventTimer.stop();
    if (m_event.isEmpty()) return;
    m_event.clear(); emit eventChanged();
}
void MatchNotices::refresh()
{
    const auto game = m_store.matchState();
    const auto event = m_store.roleState().lastEvent;
    const auto &g = game.stage.source;
    const auto &e = event.id.source;
    const auto &s = e.received && (!g.received || e.connection > g.connection) ? e : g;
    const QString epoch = s.received ? QStringLiteral("%1/%2/%3").arg(s.sessionId).arg(s.connection).arg(s.roundRevision) : QString{};
    if (m_epoch != epoch) {
        m_epoch = epoch; m_crossed.clear(); m_active.clear(); m_previousElapsed.reset(); clearEvent();
    }
    if (!game.stage.isFresh() || *game.stage.value != GameStage::Battle ||
        !game.paused.isFresh() || *game.paused.value) {
        setTimed({}); clearEvent(); m_previousElapsed.reset(); m_active.clear();
        if (game.stage.isFresh() && int(*game.stage.value) < int(GameStage::Battle)) m_crossed.clear();
        return;
    }
    if (!game.elapsedSeconds.isFresh() || !game.remainingSeconds.isFresh() ||
        !sameEpoch(game.elapsedSeconds.source, g) || !sameEpoch(game.remainingSeconds.source, g) ||
        *game.elapsedSeconds.value < 0 || *game.elapsedSeconds.value > 420 ||
        qint64(*game.elapsedSeconds.value) + *game.remainingSeconds.value != 420) {
        // 固定节点只适用于七分钟比赛。时钟不完整/跳断后不补播过去节点。
        setTimed({}); m_previousElapsed.reset(); m_active.clear(); return;
    }
    const auto &clock = game.elapsedSeconds.source;
    if (m_previousElapsed && (!sameEpoch(clock, m_clockSource) ||
        clock.receivedMonotonicNs - m_clockSource.receivedMonotonicNs > clock.ttlMs.value_or(3000) * 1000000 ||
        *game.elapsedSeconds.value < *m_previousElapsed)) {
        m_previousElapsed.reset(); m_active.clear();
    }
    const int elapsed = *game.elapsedSeconds.value;
    QVariantList timed;
    for (const auto &node : nodes) {
        if (elapsed >= node.elapsed && !m_crossed.contains(node.elapsed)) {
            const bool continuousCrossing = m_previousElapsed && *m_previousElapsed < node.elapsed &&
                elapsed - *m_previousElapsed <= 2;
            if (continuousCrossing || (node.elapsed == 0 && elapsed == 0)) m_active.insert(node.elapsed);
            m_crossed.insert(node.elapsed);
        }
        if (elapsed > node.elapsed + 3) m_active.remove(node.elapsed);
        const int left = node.elapsed - elapsed;
        const bool upcoming = left > 0 && left <= 5 && !m_crossed.contains(node.elapsed);
        if (!upcoming && !m_active.contains(node.elapsed)) continue;
        if (node.elapsed == 420 && left <= 0) continue;
        const QString target = QString::fromUtf8(node.text);
        const QString text = node.elapsed == 420 ? QStringLiteral("本局比赛剩余 %1 秒").arg(left)
            : upcoming ? QStringLiteral("%1 秒后新增 1 次%2机会").arg(left).arg(target)
            : QStringLiteral("新增 1 次%1机会").arg(target);
        timed.append(QVariantMap{{"id", node.elapsed + 1}, {"text", text}, {"seconds", qMax(0, left)}});
    }
    m_previousElapsed = elapsed; m_clockSource = clock;
    setTimed(std::move(timed));
}
void MatchNotices::observe(const QString &topic)
{
    refresh();
    if (topic != "Event") return;
    const auto game = m_store.matchState();
    const auto event = m_store.roleState().lastEvent;
    const auto &s = event.id.source;
    if (!game.stage.isFresh() || *game.stage.value != GameStage::Battle ||
        !game.paused.isFresh() || *game.paused.value || !sameEpoch(s, game.stage.source) ||
        s.validity != StateValidity::Observed || s.retained || !event.id.value) return;
    const int id = *event.id.value;
    // 参数为空的官方事件允许省略 param；其余事件必须通过共用文案校验。
    if ((id == 3 || id == 4 || (id >= 5 && id <= 9) || id == 15) && !event.parameter.value) return;
    auto notice = EventListModel::eventPresentation(id, event.parameter.value.value_or(QString{}), m_team);
    if (notice.isEmpty()) return;
    // 连续相同事件不重启动画/延长遮挡，能量进度不抢占仍在显示的危险警告。
    if ((!m_event.isEmpty() && m_event.value("text") == notice.value("text")) ||
        (id == 3 && m_event.value("accent") == QString::fromLatin1(ui::TeamRed))) return;
    notice.insert("token", ++m_sequence);
    m_event = std::move(notice);
    m_eventTimer.start(5000); emit eventChanged();
}

}
