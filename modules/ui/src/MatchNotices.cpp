#include "UiTheme.hpp"
#include "MatchNotices.hpp"

#include <array>
#include <cmath>
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
    m_runeTimer.setSingleShot(true);
    m_dartTimer.setSingleShot(true);
    connect(&m_runeTimer, &QTimer::timeout, this, [this] { m_rune.clear(); emit resultsChanged(); });
    connect(&m_dartTimer, &QTimer::timeout, this, [this] { m_dart.clear(); emit resultsChanged(); });
    connect(&store, &StateStore::changed, this, &MatchNotices::observe);
    refresh();
}
void MatchNotices::setTimed(QVariantList value)
{
    if (m_timed == value) return;
    m_timed = std::move(value); emit timedChanged();
}
QVariantList MatchNotices::results() const
{
    QVariantList result;
    if (!m_rune.isEmpty()) result.append(m_rune);
    if (!m_dart.isEmpty()) result.append(m_dart);
    return result;
}
void MatchNotices::clearResults()
{
    m_runeTimer.stop(); m_dartTimer.stop();
    if (m_rune.isEmpty() && m_dart.isEmpty()) return;
    m_rune.clear(); m_dart.clear(); emit resultsChanged();
}
void MatchNotices::showResult(QVariantMap value, bool rune)
{
    auto &current = rune ? m_rune : m_dart;
    if (current.value("text") == value.value("text") && current.value("detail") == value.value("detail")) return;
    // 同一能量结果补齐统计时原地更新，不将完成事件当作持续进度重播。
    const bool enrichment = rune && !current.isEmpty() && current.value("text") == value.value("text");
    value.insert("token", enrichment ? current.value("token") : QVariant::fromValue(++m_sequence));
    current = std::move(value);
    (rune ? m_runeTimer : m_dartTimer).start(rune ? 6000 : 5000);
    emit resultsChanged();
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
        m_epoch = epoch; m_crossed.clear(); m_active.clear(); m_previousElapsed.reset(); clearResults();
    }
    if (!game.stage.isFresh() || *game.stage.value != GameStage::Battle ||
        !game.paused.isFresh() || *game.paused.value) {
        setTimed({}); clearResults(); m_previousElapsed.reset(); m_active.clear();
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
        // 机关/闸门仅在节点到达后提示可用，提前倒计时只保留比赛最后五秒。
        const bool ending = node.elapsed == 420 && left > 0 && left <= 5;
        if (!ending && !m_active.contains(node.elapsed)) continue;
        if (node.elapsed == 420 && left <= 0) continue;
        const QString target = QString::fromUtf8(node.text);
        const QString action = node.elapsed == 30 || node.elapsed == 240 ? QStringLiteral("可开启") : QStringLiteral("可激活");
        const QString text = node.elapsed == 420 ? QStringLiteral("本局比赛剩余 %1 秒").arg(left)
            : action + target;
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
    if ((id != 3 && id != 4 && id != 9) || !event.parameter.value) return;
    QString parameter = event.parameter.value->trimmed();
    parameter.replace(QStringLiteral("，"), QStringLiteral(","));
    const auto parts = parameter.split(',');
    // 用户确认能量机关完成事件只发送己方结果，统一作为己方收益展示。
    QVariantMap notice{{"id", id}, {"kind", id == 9 ? "dart" : "rune"}, {"accent", ui::Positive}};
    if (id == 3) {
        if (parts.size() != 2) return;
        bool armsOk = false, ringsOk = false;
        const int arms = parts[0].trimmed().toInt(&armsOk);
        const double rings = parts[1].trimmed().toDouble(&ringsOk);
        if (!armsOk || !ringsOk || arms < 0 || !std::isfinite(rings) || rings < 0) return;
        notice.insert("text", QStringLiteral("大能量机关已激活"));
        notice.insert("detail", QStringLiteral("成功灯臂 %1 · 平均环数 %2").arg(arms).arg(rings, 0, 'f', 1));
        notice.insert("arms", arms); notice.insert("rings", rings);
    } else if (id == 4) {
        bool ok = false; const int type = parameter.toInt(&ok);
        if (!ok || (type != 1 && type != 2)) return;
        // 3/4 的到达顺序不固定，类型通知不覆盖仍在展示的大能量结果统计。
        if (type == 2 && m_rune.contains("arms")) return;
        notice.insert("text", type == 1 ? QStringLiteral("小能量机关已激活") : QStringLiteral("大能量机关已激活"));
    } else {
        if (parts.size() != 2) return;
        bool teamOk = false, targetOk = false;
        const int team = parts[0].trimmed().toInt(&teamOk), target = parts[1].trimmed().toInt(&targetOk);
        const QStringList targets{QStringLiteral("前哨站"), QStringLiteral("基地固定目标"), QStringLiteral("基地随机固定目标"),
            QStringLiteral("基地随机移动目标"), QStringLiteral("基地末端移动目标")};
        if (!teamOk || !targetOk || (team != 1 && team != 2) || target < 1 || target > targets.size()) return;
        const bool ally = (team == 1) == (m_team == Team::Red);
        notice.insert("accent", ally ? ui::Positive : ui::TeamRed);
        notice.insert("text", ally ? QStringLiteral("己方飞镖命中") : QStringLiteral("敌方飞镖命中"));
        notice.insert("detail", targets[target - 1]); notice.insert("target", target); notice.insert("team", team);
    }
    showResult(std::move(notice), id != 9);
}

}
