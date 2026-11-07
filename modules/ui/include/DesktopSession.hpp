#pragma once

#include "TerminalRuntime.hpp"
#include "TerminalViewModel.hpp"
#include <QObject>

namespace ly27 {

// 桌面设置通过运行层切换身份，并替换持有旧身份的展示适配。
class DesktopSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString seat READ seat NOTIFY settingsChanged)
    Q_PROPERTY(QString team READ team NOTIFY settingsChanged)
    Q_PROPERTY(QString error READ error NOTIFY settingsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY settingsChanged)
    Q_PROPERTY(QImage videoFrame READ videoFrame NOTIFY videoChanged)
    Q_PROPERTY(bool videoAvailable READ videoAvailable NOTIFY videoChanged)
    Q_PROPERTY(qreal videoAspect READ videoAspect NOTIFY videoChanged)
    Q_PROPERTY(QString videoStatus READ videoStatus NOTIFY videoChanged)
public:
    DesktopSession(TerminalRuntime &runtime, QString redTeam, QString blueTeam, QObject *parent = nullptr);
    TerminalViewModel *viewModel() const { return m_model; }
    QString seat() const;
    QString team() const { return teamName(m_runtime.config().identity.team); }
    QString error() const { return m_error; }
    bool busy() const { return m_busy; }
    QImage videoFrame() const { return m_videoFrame; }
    bool videoAvailable() const { return !m_videoFrame.isNull(); }
    qreal videoAspect() const { return m_videoAspect; }
    QString videoStatus() const;
    QString windowTitle() const;
    Q_INVOKABLE bool selectIdentity(const QString &seat, const QString &team);
signals:
    void settingsChanged();
    void viewModelChanged();
    void videoChanged();
private:
    TerminalRuntime &m_runtime;
    QString m_redTeam;
    QString m_blueTeam;
    TerminalViewModel *m_model = nullptr;
    QString m_error;
    bool m_busy = false;
    QImage m_videoFrame;
    QString m_videoState;
    qreal m_videoAspect = 16.0 / 9.0;
    void rebuildViewModel();
    void updateVideo();
};

}
