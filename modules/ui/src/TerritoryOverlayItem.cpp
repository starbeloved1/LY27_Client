#include "TerritoryOverlayItem.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QQuickWindow>
#include <cmath>

namespace ly27 {

namespace {
const QStringList territoryKeys{"rOutpost", "bOutpost", "redBase", "blueBase"};
constexpr std::array<int, 6> regionTerritories{0, 1, 2, 2, 3, 3};

QColor blendColor(const QColor &from, const QColor &to, qreal progress)
{
    const auto mix = [progress](qreal a, qreal b) { return a + (b - a) * progress; };
    return QColor::fromRgbF(mix(from.redF(), to.redF()), mix(from.greenF(), to.greenF()),
                            mix(from.blueF(), to.blueF()), mix(from.alphaF(), to.alphaF()));
}
}

const QList<QPolygonF> &TerritoryOverlayItem::referenceRegions()
{
    // 依据规则表 5-20 与图 5-23，沿当前高清底图校准；不用于模拟 RFID 检测。
    static const QList<QPolygonF> regions{
        // 红方前哨站增益点与相邻中央高地增益点合并。
        {{766, 163}, {770, 208}, {768, 248}, {686, 368}, {686, 577}, {724, 659},
         {724, 710}, {690, 746}, {649, 746}, {665, 719}, {648, 703}, {640, 684},
         {651, 658}, {634, 596}, {634, 353}},
        // 蓝方对应区域按底图独立校准，避免 AI 重建图局部不对称造成偏移。
        {{1038, 182}, {1007, 182}, {972, 217}, {972, 256}, {1012, 343}, {1012, 556},
         {928, 678}, {928, 746}, {945, 746}, {1064, 575}, {1064, 329}, {1045, 275},
         {1050, 247}, {1046, 218}, {1056, 205}, {1086, 151}, {1064, 151}},
        // 基地阵地的基地增益点与堡垒增益点分开画，不把两点之间道路计入范围。
        {{78, 394}, {159, 347}, {276, 414}, {276, 511}, {162, 576}, {78, 527}},
        {{376, 397}, {454, 397}, {487, 461}, {454, 526}, {376, 526}, {344, 461}},
        {{1417, 418}, {1534, 348}, {1609, 395}, {1609, 527}, {1528, 575}, {1417, 512}},
        {{1242, 397}, {1320, 397}, {1353, 461}, {1320, 526}, {1242, 526}, {1209, 461}}
    };
    return regions;
}

TerritoryOverlayItem::TerritoryOverlayItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    for (int index = 0; index < int(m_transitions.size()); ++index) {
        auto &transition = m_transitions[index];
        transition.current = transition.from = transition.target = styleForState({});
        transition.animation = new QVariantAnimation(this);
        transition.animation->setObjectName("territoryTransition_" + territoryKeys[index]);
        transition.animation->setDuration(600);
        transition.animation->setStartValue(0.0);
        transition.animation->setEndValue(1.0);
        transition.animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(transition.animation, &QVariantAnimation::valueChanged, this, [this, index](const QVariant &value) {
            auto &t = m_transitions[index];
            const qreal progress = value.toReal();
            t.current = {blendColor(t.from.border, t.target.border, progress),
                         blendColor(t.from.band, t.target.band, progress),
                         t.from.dashed + (t.target.dashed - t.from.dashed) * progress};
            update();
        });
    }
    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, [this] { setPhase(m_phase + m_elapsed.restart() / 8000.0); });
    connect(this, &QQuickItem::visibleChanged, this, &TerritoryOverlayItem::synchronizeTimer);
    const auto observeWindow = [this](QQuickWindow *window) {
        disconnect(m_windowVisibility);
        if (window) m_windowVisibility = connect(window, &QWindow::visibleChanged, this, &TerritoryOverlayItem::synchronizeTimer);
        synchronizeTimer();
    };
    connect(this, &QQuickItem::windowChanged, this, observeWindow);
    // 直接挂在窗口下创建时，构造前已有关联窗口，不能漏掉首次显示通知。
    observeWindow(window());
}

void TerritoryOverlayItem::setBlueView(bool value)
{
    if (m_blueView == value) return;
    m_blueView = value; update(); emit blueViewChanged();
}

void TerritoryOverlayItem::setPhase(qreal value)
{
    if (!std::isfinite(value)) return;
    value -= std::floor(value);
    if (qFuzzyCompare(m_phase, value)) return;
    m_phase = value; update(); emit phaseChanged();
}

void TerritoryOverlayItem::setAnimating(bool value)
{
    if (m_animating == value) return;
    m_animating = value; synchronizeTimer(); emit animatingChanged();
}

void TerritoryOverlayItem::setTerritories(const QVariantMap &value)
{
    if (m_territories == value) return;
    for (int index = 0; index < int(m_transitions.size()); ++index) {
        const auto previous = m_territories.value(territoryKeys[index]).toMap();
        const auto next = value.value(territoryKeys[index]).toMap();
        auto &t = m_transitions[index];
        const auto target = styleForState(next);
        // 倒计时与普通刷新不重启动效；未知、断线或首帧不能带出历史归属。
        const bool changed = previous.value("known") != next.value("known")
            || previous.value("controller") != next.value("controller")
            || previous.value("temporary") != next.value("temporary");
        if (!changed) continue;
        t.animation->stop();
        t.from = t.current;
        t.target = target;
        if (m_timer.isActive() && previous.value("known").toBool() && next.value("known").toBool()) {
            // 连续切换从当前中间帧继续，不能跳回上一次完整颜色。
            t.animation->start();
        } else {
            t.current = t.from = target;
        }
    }
    m_territories = value; update(); emit territoriesChanged();
}

QVariantMap TerritoryOverlayItem::regionState(int index) const
{
    return index >= 0 && index < regionCount()
        ? m_territories.value(territoryKeys[regionTerritories[index]]).toMap() : QVariantMap{};
}

TerritoryOverlayItem::VisualStyle TerritoryOverlayItem::styleForState(const QVariantMap &state)
{
    if (!state.value("known").toBool()) return {QColor("#9AA8B4"), QColor(228, 235, 242, 145), 0};
    const bool red = state.value("controller").toString() == "red";
    return {QColor(red ? "#FF737A" : "#62C9FF"),
            red ? QColor(255, 242, 244, 205) : QColor(240, 250, 255, 205),
            state.value("temporary").toBool() ? 1.0 : 0.0};
}

void TerritoryOverlayItem::finishTransitions()
{
    for (auto &t : m_transitions) {
        t.animation->stop();
        t.current = t.from = t.target;
    }
    update();
}

void TerritoryOverlayItem::synchronizeTimer()
{
    const bool running = m_animating && isVisible() && window() && window()->isVisible();
    if (running && !m_timer.isActive()) { m_elapsed.start(); m_timer.start(); }
    else if (!running) { m_timer.stop(); m_elapsed.invalidate(); finishTransitions(); }
}

QPainterPath TerritoryOverlayItem::regionPath(int index) const
{
    QPainterPath path;
    if (index < 0 || index >= regionCount() || width() <= 0 || height() <= 0) return path;
    path.addPolygon(referenceRegions()[index]); path.closeSubpath();
    QTransform transform;
    if (m_blueView) { transform.translate(width(), height()); transform.scale(-1, -1); }
    transform.scale(width() / 1662.0, height() / 946.0);
    return transform.map(path);
}

void TerritoryOverlayItem::paint(QPainter *painter)
{
    if (width() <= 0 || height() <= 0) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipRect(boundingRect());
    const qreal spacing = qBound(7.0, width() * 0.013, 16.0);
    const qreal lineWidth = qBound(1.1, width() * 0.002, 2.2);
    constexpr qreal slope = .65;
    const qreal interceptSpacing = spacing * std::sqrt(1 + slope * slope);
    constexpr qreal tau = 6.283185307179586;
    for (int index = 0; index < regionCount(); ++index) {
        const auto path = regionPath(index);
        const auto bounds = path.boundingRect();
        const auto state = regionState(index);
        const bool known = state.value("known").toBool();
        const auto &visual = m_transitions[regionTerritories[index]].current;
        painter->fillPath(path, QColor(235, 248, 255, 7));
        painter->save();
        painter->setClipPath(path, Qt::IntersectClip);
        QPainterPath bands;
        // 光带在轮廓内连续流动，轻微正弦波动；周期接缝为整数行距，不闪回。
        const qreal offset = std::fmod(m_phase * 6, 1.0) * interceptSpacing;
        const qreal first = std::floor((bounds.top() + slope * bounds.left()) / interceptSpacing)
            * interceptSpacing + offset - interceptSpacing;
        for (qreal intercept = first; intercept < bounds.bottom() + slope * bounds.right() + interceptSpacing; intercept += interceptSpacing) {
            bands.moveTo(bounds.left() - 6, intercept - slope * (bounds.left() - 6)
                + std::sin((bounds.left() - 6) * .025 + m_phase * tau) * 1.3);
            for (qreal x = bounds.left(); x <= bounds.right() + 12; x += 8)
                bands.lineTo(x, intercept - slope * x + std::sin(x * .025 + m_phase * tau) * 1.3);
        }
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(245, 252, 255, 35), lineWidth + 5)); painter->drawPath(bands);
        painter->setPen(QPen(QColor(250, 253, 255, 65), lineWidth + 2)); painter->drawPath(bands);
        painter->setPen(QPen(visual.band, lineWidth)); painter->drawPath(bands);
        painter->restore();
        // 多层细描边形成柔光，不遮住机器人标记，也不依赖整幅纹理模糊。
        painter->setBrush(Qt::NoBrush);
        for (const auto &stroke : {QPair<qreal, int>{11, 15}, {6, 35}, {3, 75}, {1.7, 245}}) {
            QColor color = visual.border; color.setAlpha(known ? stroke.second : stroke.second / 2);
            QPen border(color, stroke.first, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            // 各层使用相同像素长度的虚线，不能随描边宽度扩大虚线周期。
            // 逐渐打开或收拢间隙，实线与虚线共享同一路径，不整层闪烁或突然替换。
            if (visual.dashed > .001) {
                const qreal gap = 7 * visual.dashed;
                border.setDashPattern({(17 - gap) / stroke.first, gap / stroke.first});
            }
            painter->setPen(border);
            painter->drawPath(path);
        }
        const QString advancing = state.value("advancing").toString();
        const qint64 seconds = state.value("seconds", -1).toLongLong();
        if (!advancing.isEmpty() && seconds > 0) {
            const QColor actor = advancing == "red" ? QColor("#FF737A") : QColor("#62C9FF");
            QPen highlights(actor, 2.5, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
            highlights.setDashPattern({4, 18}); highlights.setDashOffset(-m_phase * 44);
            painter->setPen(highlights); painter->drawPath(path);
            // 同一基地的堡垒同步高光，秒数只在基地处标一次，避免重复文字。
            if (width() >= 400 && height() >= 180 && index != 3 && index != 5) {
                // 两个前哨标签上下错开，小窗口也不能挤在中央相互遮挡。
                static const QPointF labels[]{{747, 560}, {945, 395}, {171, 608}, {}, {1514, 608}, {}};
                QPointF center(labels[index].x() / 1662 * width(), labels[index].y() / 946 * height());
                if (m_blueView) center = QPointF(width() - center.x(), height() - center.y());
                const QString text = QStringLiteral("%1推进 %2s").arg(advancing == "red" ? QStringLiteral("红方") : QStringLiteral("蓝方")).arg(seconds);
                QFont font = painter->font(); font.setPixelSize(11); font.setBold(true); painter->setFont(font);
                const qreal labelWidth = QFontMetricsF(font).horizontalAdvance(text) + 14;
                QRectF box(center.x() - labelWidth / 2, center.y() - 11, labelWidth, 22);
                box.moveLeft(qBound(2.0, box.left(), width() - box.width() - 2));
                box.moveTop(qBound(2.0, box.top(), height() - box.height() - 2));
                painter->setBrush(QColor(8, 16, 24, 235)); painter->setPen(QPen(actor, 1));
                painter->drawRoundedRect(box, 3, 3);
                painter->setPen(actor); painter->drawText(box, Qt::AlignCenter, text);
            }
        }
    }
    painter->restore();
}

}
