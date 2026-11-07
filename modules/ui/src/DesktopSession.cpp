#include "DesktopSession.hpp"

#include <utility>

namespace ly27 {

DesktopSession::DesktopSession(TerminalRuntime &runtime, QString redTeam, QString blueTeam, QObject *parent)
    : QObject(parent), m_runtime(runtime), m_redTeam(std::move(redTeam)), m_blueTeam(std::move(blueTeam))
{
    connect(&runtime, &TerminalRuntime::configurationChanged, this, &DesktopSession::rebuildViewModel);
    connect(&runtime, &TerminalRuntime::videoFrameAvailable, this, &DesktopSession::updateVideo);
    connect(&runtime, &TerminalRuntime::videoStateChanged, this, &DesktopSession::updateVideo);
    rebuildViewModel();
}

QString DesktopSession::videoStatus() const
{
    if (m_videoState == "stale") return QStringLiteral("图传信号中断");
    if (m_videoState == "error") return QStringLiteral("图传接收失败");
    if (m_videoState == "stopped" || m_videoState == "disabled") return QStringLiteral("图传未启动");
    return QStringLiteral("等待 UDP 图传");
}

void DesktopSession::updateVideo()
{
    // 接收器在工作线程解码；这里只取最新图像，停流时不继续展示过期画面。
    const auto frame = m_runtime.video().latestFrame();
    const QImage image = frame ? frame->image : QImage{};
    const QString state = m_runtime.video().snapshot().value("state").toString();
    if (image.cacheKey() == m_videoFrame.cacheKey() && state == m_videoState) return;
    m_videoFrame = image;
    m_videoState = state;
    // 短暂断流保持最后一次有效比例，避免周围面板反复跳动。
    if (!image.isNull()) m_videoAspect = qreal(image.width()) / image.height();
    emit videoChanged();
}

QString DesktopSession::seat() const
{
    const auto &identity = m_runtime.config().identity;
    if (identity.role != Role::Infantry) return roleName(identity.role);
    return QStringLiteral("infantry%1").arg(identity.robotId.value_or(3) % 100);
}

QString DesktopSession::windowTitle() const
{
    const auto &identity = m_runtime.config().identity;
    const QString role = identity.role == Role::Commander ? QStringLiteral("指挥手")
        : identity.role == Role::Heavy ? QStringLiteral("重装操作手")
        : QStringLiteral("%1号步兵操作手").arg(identity.robotId.value_or(3) % 100);
    return QStringLiteral("LY27 Client · %1 · %2")
        .arg(identity.team == Team::Red ? QStringLiteral("红方") : QStringLiteral("蓝方"), role);
}

void DesktopSession::rebuildViewModel()
{
    updateVideo();
    auto *previous = m_model;
    if (previous) previous->tactics()->cancel();
    m_model = new TerminalViewModel(m_runtime.stateStore(), m_runtime.config().identity,
                                   m_redTeam, m_blueTeam, this);
    if (previous) m_model->suppressRestoredResult();
    m_model->tactics()->setCommandDispatcher(&m_runtime.commands());
    emit viewModelChanged();
    emit settingsChanged();
    // QML 先切换到新适配，再销毁旧模型，避免旧指挥目标或通知跨身份保留。
    if (previous) previous->deleteLater();
}

bool DesktopSession::selectIdentity(const QString &seat, const QString &team)
{
    if (m_busy) return false;
    const auto selectedTeam = parseTeam(team);
    TerminalIdentity next;
    if (!selectedTeam || (seat != "heavy" && seat != "infantry3" && seat != "infantry4" && seat != "commander")) {
        m_error = QStringLiteral("请选择有效的角色和阵营"); emit settingsChanged(); return false;
    }
    if (seat == this->seat() && team == this->team()) {
        m_error.clear(); emit settingsChanged(); return true;
    }
    next.team = *selectedTeam;
    next.role = seat == "heavy" ? Role::Heavy : seat == "commander" ? Role::Commander : Role::Infantry;
    if (next.role != Role::Commander)
        next.robotId = (next.team == Team::Blue ? 100 : 0) + (seat == "heavy" ? 1 : seat == "infantry4" ? 4 : 3);
    next.stationId = team + "-" + seat;
    m_busy = true; m_error.clear(); emit settingsChanged();
    // Client ID 是独立配置，显式沿用当前值，不按车号猜测现场通信身份。
    const auto result = m_runtime.switchIdentity(next, m_runtime.config().network.mqttClientId);
    m_busy = false;
    if (!result.applied) {
        m_error = result.error;
        if (!result.rollbackError.isEmpty()) m_error += QStringLiteral("；恢复失败：") + result.rollbackError;
    }
    emit settingsChanged();
    return result.applied;
}

}
