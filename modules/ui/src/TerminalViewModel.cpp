#include "TerminalViewModel.hpp"

#include <algorithm>
#include <utility>

namespace ly27 {
namespace {
template<class T> QVariant number(const StateField<T> &field)
{
    return field.isFresh() ? QVariant::fromValue(*field.value) : QVariant(-1);
}
QString stageName(const StateField<GameStage> &stage)
{
    if (!stage.isFresh()) return QStringLiteral("等待比赛数据");
    switch (*stage.value) {
    case GameStage::NotStarted: return QStringLiteral("比赛未开始");
    case GameStage::Preparation: return QStringLiteral("准备阶段");
    case GameStage::SelfCheck: return QStringLiteral("裁判系统自检");
    case GameStage::Countdown: return QStringLiteral("开局倒计时");
    case GameStage::Battle: return QStringLiteral("比赛进行中");
    case GameStage::Settlement: return QStringLiteral("比赛结算");
    }
    return QStringLiteral("未知阶段");
}
QString baseStatusText(const BaseState &base)
{
    if (base.currentHealth.isFresh() && *base.currentHealth.value == 0)
        return QStringLiteral("基地已摧毁");
    if (!base.defenseState.isFresh()) return QStringLiteral("状态未知");
    switch (*base.defenseState.value) {
    case 0: return QStringLiteral("护盾激活"); // 官方枚举：拥有 100% 防御增益。
    case 1: return QStringLiteral("护甲未展开");
    case 2: return QStringLiteral("护甲展开");
    }
    return QStringLiteral("未知状态");
}
}

TerminalViewModel::TerminalViewModel(const StateStore &store, TerminalIdentity identity,
                                   QString redTeam, QString blueTeam, QObject *parent)
    : QObject(parent), m_store(store), m_identity(std::move(identity)),
      m_redTeam(std::move(redTeam)), m_blueTeam(std::move(blueTeam)),
      m_blueView(m_identity.team == Team::Blue), m_robots(store, m_identity, this),
      m_events(store, m_identity.team, this)
{
    connect(&store, &StateStore::changed, this, [this](const QString &) { refresh(); });
    connect(&m_timer, &QTimer::timeout, this, &TerminalViewModel::refresh);
    m_timer.start(100);
    refresh();
}

void TerminalViewModel::setBlueView(bool value)
{
    if (m_blueView == value) return;
    m_blueView = value;
    emit blueViewChanged();
    m_robots.refresh(value);
}

void TerminalViewModel::refresh()
{
    const auto game = m_store.matchState();
    const auto global = m_store.globalState();
    const bool redAlly = m_identity.team == Team::Red;
    const auto &redBase = redAlly ? global.allyBase : global.enemyBase;
    const auto &blueBase = redAlly ? global.enemyBase : global.allyBase;
    const bool paused = game.paused.isFresh() && *game.paused.value;
    QString timer = QStringLiteral("--:--");
    if (game.remainingSeconds.isFresh()) {
        const int seconds = std::max(0, *game.remainingSeconds.value);
        timer = QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0'))
                                      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
    const QVariantMap values{
        {"redTeam", m_redTeam}, {"blueTeam", m_blueTeam}, {"timer", timer},
        {"currentRound", number(game.currentRound)}, {"totalRounds", number(game.totalRounds)},
        {"redScore", number(game.redScore)}, {"blueScore", number(game.blueScore)},
        {"stage", paused ? QStringLiteral("比赛暂停") : stageName(game.stage)},
        {"paused", paused}, {"fresh", game.remainingSeconds.isFresh()},
        {"redBaseHp", number(redBase.currentHealth)}, {"blueBaseHp", number(blueBase.currentHealth)},
        {"redBaseState", number(redBase.defenseState)}, {"blueBaseState", number(blueBase.defenseState)},
        {"redBaseStatusText", baseStatusText(redBase)}, {"blueBaseStatusText", baseStatusText(blueBase)},
        {"allyEconomy", number(global.logistics.remainingEconomy)},
        {"allyTeamLabel", redAlly ? QStringLiteral("红方") : QStringLiteral("蓝方")},
        {"redControlPoints", number(redAlly ? global.allyControlPoints : global.enemyControlPoints)},
        {"blueControlPoints", number(redAlly ? global.enemyControlPoints : global.allyControlPoints)},
        {"rOutpostHp", number(global.rOutpost.currentHealth)}, {"bOutpostHp", number(global.bOutpost.currentHealth)}
    };
    if (m_match != values) { m_match = values; emit changed(); }
    m_robots.refresh(m_blueView);
}

}
