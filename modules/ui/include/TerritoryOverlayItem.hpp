#pragma once

#include <QElapsedTimer>
#include <QMetaObject>
#include <QPolygonF>
#include <QPainterPath>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantMap>
#include <QVariantAnimation>
#include <array>

namespace ly27 {

// 阵地范围与占领状态叠加层；读取展示适配，不解析协议或根据几何推导占领。
class TerritoryOverlayItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool blueView READ blueView WRITE setBlueView NOTIFY blueViewChanged)
    Q_PROPERTY(qreal phase READ phase WRITE setPhase NOTIFY phaseChanged)
    Q_PROPERTY(bool animating READ animating WRITE setAnimating NOTIFY animatingChanged)
    Q_PROPERTY(int regionCount READ regionCount CONSTANT)
    Q_PROPERTY(QVariantMap territories READ territories WRITE setTerritories NOTIFY territoriesChanged)
public:
    explicit TerritoryOverlayItem(QQuickItem *parent = nullptr);
    bool blueView() const { return m_blueView; }
    void setBlueView(bool value);
    qreal phase() const { return m_phase; }
    void setPhase(qreal value);
    bool animating() const { return m_animating; }
    void setAnimating(bool value);
    int regionCount() const { return referenceRegions().size(); }
    QVariantMap territories() const { return m_territories; }
    void setTerritories(const QVariantMap &value);
    QVariantMap regionState(int index) const;
    // 使用现有 1662×946 底图像素坐标；前两项为前哨，后四项为红蓝基地和堡垒。
    static const QList<QPolygonF> &referenceRegions();
    QPainterPath regionPath(int index) const;
    void paint(QPainter *painter) override;

signals:
    void blueViewChanged();
    void phaseChanged();
    void animatingChanged();
    void territoriesChanged();

private:
    struct VisualStyle {
        QColor border;
        QColor band;
        qreal dashed = 0;
    };
    struct Transition {
        VisualStyle current;
        VisualStyle from;
        VisualStyle target;
        QVariantAnimation *animation = nullptr;
    };
    // 四个逻辑阵地各维护一次过渡，基地与堡垒共用同一视觉状态。
    std::array<Transition, 4> m_transitions;
    static VisualStyle styleForState(const QVariantMap &state);
    void finishTransitions();
    bool m_blueView = false;
    bool m_animating = true;
    qreal m_phase = 0;
    QVariantMap m_territories;
    QTimer m_timer{this};
    QElapsedTimer m_elapsed;
    QMetaObject::Connection m_windowVisibility;
    void synchronizeTimer();
};

}
