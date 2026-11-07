#pragma once

#include <QQuickView>
#include <QPointer>

namespace ly27 {

class TerminalViewModel;
class TacticalMapController;
class DesktopSession;

class MapWindow final : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(bool injuryPanelVisible READ injuryPanelVisible NOTIFY injuryPanelVisibleChanged)
    Q_PROPERTY(bool fullScreen READ fullScreen WRITE setFullScreen NOTIFY fullScreenChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible WRITE setSettingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(bool videoVisible READ videoVisible NOTIFY videoVisibleChanged)
public:
    explicit MapWindow(TerminalViewModel *model = nullptr, QWindow *parent = nullptr, DesktopSession *session = nullptr);
    bool injuryPanelVisible() const { return m_injuryPanelVisible; }
    bool fullScreen() const { return visibility() == QWindow::FullScreen; }
    void setFullScreen(bool enabled);
    bool settingsVisible() const { return m_settingsVisible; }
    void setSettingsVisible(bool visible);
    bool videoVisible() const { return m_videoVisible; }

signals:
    void injuryPanelVisibleChanged();
    void fullScreenChanged();
    void settingsVisibleChanged();
    void videoVisibleChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    QPointer<TacticalMapController> m_tactics;
    bool m_injuryPanelVisible = false;
    bool m_settingsVisible = false;
    bool m_videoVisible = false;
    QWindow::Visibility m_windowedVisibility = QWindow::Windowed;
    QRect m_windowedGeometry;
    void setInjuryPanelVisible(bool visible);
};

}
