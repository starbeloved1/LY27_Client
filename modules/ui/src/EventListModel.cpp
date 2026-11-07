#include "UiTheme.hpp"
#include "EventListModel.hpp"

#include <QDateTime>
#include <cmath>
#include <utility>

namespace ly27 {
namespace {
const QString normal = QString::fromLatin1(ui::Text);
const QString danger = QString::fromLatin1(ui::TeamRed);
const QString success = QString::fromLatin1(ui::Positive);
QString robotName(int id)
{
    QString name;
    switch (id % 100) {
    case 1: name = QStringLiteral("重装"); break;
    case 3: name = QStringLiteral("3号步兵"); break;
    case 4: name = QStringLiteral("4号步兵"); break;
    case 6: name = QStringLiteral("空中系统"); break;
    case 7: name = QStringLiteral("哨兵"); break;
    case 8: name = QStringLiteral("飞镖"); break;
    case 9: name = QStringLiteral("雷达"); break;
    default: name = QStringLiteral("%1号机器人").arg(id % 100); break;
    }
    return (id >= 100 ? QStringLiteral("蓝方") : QStringLiteral("红方")) + name;
}
QString clockText(int seconds)
{
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0'))
                                  .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
}

EventListModel::EventListModel(const StateStore &store, Team team, QObject *parent)
    : QAbstractListModel(parent), m_store(store), m_team(team)
{
    // 晚于后端创建视图时恢复已有历史，不用当前比赛时间伪造过去的事件时间。
    for (const auto &notice : store.notifications())
        if (const auto *event = std::get_if<EventNotice>(&notice)) append(*event, false);
    observe({});
    connect(&store, &StateStore::changed, this, &EventListModel::observe);
}
int EventListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}
QHash<int, QByteArray> EventListModel::roleNames() const
{
    return {{Time, "eventTime"}, {Text, "eventText"}, {Color, "eventColor"},
            {EventId, "eventId"}, {Parameter, "eventParameter"}, {TimeSource, "eventTimeSource"}};
}
QVariant EventListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    const auto &r = m_rows.at(index.row());
    switch (role) {
    case Time: return r.time;
    case Text: return r.text;
    case Color: return r.color;
    case EventId: return r.id;
    case Parameter: return r.parameter;
    case TimeSource: return r.timeSource;
    }
    return {};
}
void EventListModel::clear()
{
    if (!m_rows.isEmpty()) { beginResetModel(); m_rows.clear(); endResetModel(); }
    m_session.clear();
    m_totalEconomy.reset();
    m_economySource = {};
}
void EventListModel::observe(const QString &topic)
{
    if (!topic.isEmpty() && topic != "Event" && topic != "GameStatus" && topic != "GlobalLogisticsStatus") return;
    const auto event = m_store.roleState().lastEvent;
    const auto game = m_store.matchState();
    const auto economy = m_store.globalState().logistics.totalEconomyObtained;
    const StateSource *context = nullptr;
    for (const auto *candidate : {&event.id.source, &game.stage.source, &economy.source}) {
        if (candidate->received && (!context || candidate->connection > context->connection ||
            (candidate->connection == context->connection && candidate->receivedMonotonicNs >= context->receivedMonotonicNs)))
            context = candidate;
    }
    if (!context) { clear(); return; }
    if (m_session != context->sessionId || m_roundRevision != context->roundRevision) clear();
    m_session = context->sessionId;
    m_roundRevision = context->roundRevision;
    if (topic == "GlobalLogisticsStatus" || topic.isEmpty()) observeEconomy();
    if (topic != QStringLiteral("Event")) return;
    const auto &source = event.id.source;
    if (source.validity != StateValidity::Observed || source.retained) return;
    // 不按内容或时间去重：两次相同的合法事件仍是两条通知。
    append(event, true);
}
void EventListModel::append(const EventNotice &event, bool live)
{
    const auto &source = event.id.source;
    if (!event.id.value || !source.received || source.retained) return;
    auto row = describe(*event.id.value, event.parameter.value.value_or(QString{}), m_team);
    appendRow(std::move(row), source, live);
}
void EventListModel::appendRow(Row row, const StateSource &source, bool live)
{
    m_session = source.sessionId;
    m_roundRevision = source.roundRevision;
    const auto game = m_store.matchState();
    const auto &time = game.remainingSeconds;
    if (live && time.isFresh() && *time.value >= 0 && time.source.sessionId == source.sessionId &&
        time.source.connection == source.connection && time.source.roundRevision == source.roundRevision &&
        time.source.receivedMonotonicNs <= source.receivedMonotonicNs) {
        row.time = clockText(*time.value);
        row.timeSource = QStringLiteral("比赛剩余时间");
    } else {
        row.time = QDateTime::fromMSecsSinceEpoch(source.receivedUtcMs).toLocalTime().toString("HH:mm:ss");
        row.timeSource = QStringLiteral("接收时间");
    }
    if (m_rows.size() == Limit) {
        beginRemoveRows({}, 0, 0); m_rows.removeFirst(); endRemoveRows();
    }
    beginInsertRows({}, m_rows.size(), m_rows.size()); m_rows.append(row); endInsertRows();
}
void EventListModel::observeEconomy()
{
    const auto total = m_store.globalState().logistics.totalEconomyObtained;
    if (!total.isFresh()) { m_totalEconomy.reset(); return; }
    const auto &s = total.source;
    const bool continuous = m_totalEconomy && s.sessionId == m_economySource.sessionId &&
        s.connection == m_economySource.connection && s.roundRevision == m_economySource.roundRevision &&
        s.receivedMonotonicNs >= m_economySource.receivedMonotonicNs &&
        s.receivedMonotonicNs - m_economySource.receivedMonotonicNs < s.ttlMs.value_or(3000) * 1000000;
    const auto game = m_store.matchState();
    if (continuous && *total.value > *m_totalEconomy && game.stage.isFresh() &&
        *game.stage.value == GameStage::Battle && game.stage.source.connection == s.connection &&
        game.stage.source.roundRevision == s.roundRevision) {
        Row row;
        row.text = QStringLiteral("获得%1金币").arg(*total.value - *m_totalEconomy);
        row.color = QStringLiteral("#FFFFFF");
        row.parameter = QString::number(*total.value - *m_totalEconomy);
        // 本地收入观测使用编号 0，与官方 Event 1–16 分开。
        appendRow(std::move(row), s, true);
    }
    m_totalEconomy = total.value;
    m_economySource = s;
}
QVariantMap EventListModel::eventPresentation(int id, const QString &parameter, Team team)
{
    const auto row = describe(id, parameter, team);
    if (id < 3 || id > 16 || row.text.startsWith(QStringLiteral("事件 #"))) return {};
    return {{"id", id}, {"text", row.text}, {"accent", row.color}};
}
EventListModel::Row EventListModel::describe(int id, const QString &parameter, Team team)
{
    Row row;
    row.id = id;
    row.parameter = parameter.left(256);
    if (parameter.size() > 256) row.parameter += QStringLiteral("…");
    QString p = row.parameter.trimmed();
    p.replace(QStringLiteral("，"), QStringLiteral(","));
    const auto parts = p.split(',');
    bool ok = false;
    const int value = p.toInt(&ok);
    const bool redAlly = team == Team::Red;
    auto favorable = [&](bool red) { return red == redAlly ? success : danger; };
    row.color = normal;
    row.text = QStringLiteral("事件 #%1").arg(id);
    if (!row.parameter.isEmpty()) row.text += QStringLiteral("：") + row.parameter;
    switch (id) {
    case 1: {
        if (parts.size() != 2) break;
        bool a = false, b = false;
        const int victim = parts[0].trimmed().toInt(&a), killer = parts[1].trimmed().toInt(&b);
        if (!a || !b || victim <= 0 || victim >= 200 || victim == 100 || killer < 0 || killer >= 200 || killer == 100) break;
        row.text = killer == 0 || killer == victim || killer % 100 == 10
            ? QStringLiteral("%1阵亡").arg(robotName(victim))
            : QStringLiteral("%1击毁了%2").arg(robotName(killer), robotName(victim));
        row.color = favorable(victim >= 100);
        break;
    }
    case 2:
        if (ok && (value == 11 || value == 111)) {
            row.text = value == 11 ? QStringLiteral("红方前哨站被摧毁") : QStringLiteral("蓝方前哨站被摧毁");
            row.color = favorable(value == 111);
        }
        break;
    case 3: {
        if (parts.size() != 2) break;
        bool a = false, b = false;
        const int arms = parts[0].trimmed().toInt(&a);
        const double rings = parts[1].trimmed().toDouble(&b);
        if (a && b && arms >= 0 && std::isfinite(rings) && rings >= 0)
            row.text = QStringLiteral("大能量机关：已激活%1个灯臂，平均环数%2").arg(arms).arg(rings, 0, 'f', 1);
        break;
    }
    case 4:
        if (ok && (value == 1 || value == 2)) {
            // 该事件只有激活类型参数，不沿用旧版对阵营的隐含假设。
            row.text = QStringLiteral("%1能量机关已激活").arg(value == 1 ? QStringLiteral("小") : QStringLiteral("大"));
        }
        break;
    case 5: case 6:
        if (ok && value >= 1 && value <= 4) {
            const bool red = value <= 2;
            row.text = QStringLiteral("%1前哨阵地被%2方%3控制").arg(id == 5 ? "R" : "B",
                red ? QStringLiteral("红") : QStringLiteral("蓝"), value % 2 ? QStringLiteral("临时") : QStringLiteral("完全"));
            row.color = favorable(red);
        }
        break;
    case 7: case 8:
        if (ok && (value == 1 || value == 2)) {
            const bool red = id == 7 ? value == 1 : value == 2;
            row.text = QStringLiteral("%1基地阵地被%2方%3控制").arg(id == 7 ? "R" : "B",
                red ? QStringLiteral("红") : QStringLiteral("蓝"), value == 1 ? QStringLiteral("完全") : QStringLiteral("临时"));
            row.color = favorable(red);
        }
        break;
    case 9: {
        if (parts.size() != 2) break;
        bool a = false, b = false;
        const int team = parts[0].trimmed().toInt(&a), target = parts[1].trimmed().toInt(&b);
        const QStringList targets{QStringLiteral("前哨站"), QStringLiteral("基地固定目标"), QStringLiteral("基地随机固定目标"),
                                  QStringLiteral("基地随机移动目标"), QStringLiteral("基地末端移动目标")};
        if (a && b && (team == 1 || team == 2) && target >= 1 && target <= 5) {
            row.text = QStringLiteral("%1方飞镖命中%2").arg(team == 1 ? QStringLiteral("红") : QStringLiteral("蓝"), targets[target - 1]);
            row.color = favorable(team == 1);
        }
        break;
    }
    case 10: row.text = QStringLiteral("对方飞镖闸门开启"); row.color = danger; break;
    case 11: row.text = QStringLiteral("基地正在遭受攻击！"); row.color = danger; break;
    case 12: row.text = QStringLiteral("对方前哨站停转"); row.color = success; break;
    case 13: row.text = QStringLiteral("对方基地护甲展开"); row.color = success; break;
    case 14: row.text = QStringLiteral("重装战亡临时激活，进入强制退出缓冲期"); row.color = danger; break;
    case 15: {
        const QStringList results{QStringLiteral("装配成功"), QStringLiteral("能量单元拔出，装配失败"), QStringLiteral("装配超时，装配失败"),
            QStringLiteral("离开装配区过久，装配失败"), QStringLiteral("重装战亡，装配失败"), QStringLiteral("四级难度未满足协作时限，装配失败"),
            QStringLiteral("主动退出装配"), QStringLiteral("装配完成但未检测到能量单元"), QStringLiteral("缓冲期到期，装配流程强制结束")};
        if (ok && value >= 0 && value < results.size()) {
            row.text = results[value]; row.color = value == 0 ? success : danger;
        }
        break;
    }
    case 16: row.text = QStringLiteral("重装机器人成功安装模块化42mm发射机构"); row.color = success; break;
    }
    return row;
}

}
