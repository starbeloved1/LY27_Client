#pragma once

#include <QQuickView>
#include <QPointer>

namespace ly27 {

class TerminalViewModel;
class TacticalMapController;

class MapWindow final : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(bool injuryPanelVisible READ injuryPanelVisible NOTIFY injuryPanelVisibleChanged)
public:
    explicit MapWindow(TerminalViewModel *model = nullptr, QWindow *parent = nullptr);
    bool injuryPanelVisible() const { return m_injuryPanelVisible; }

signals:
    void injuryPanelVisibleChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    QPointer<TacticalMapController> m_tactics;
    bool m_injuryPanelVisible = false;
    void setInjuryPanelVisible(bool visible);
};

}
