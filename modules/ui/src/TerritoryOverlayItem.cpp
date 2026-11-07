#include "UiTheme.hpp"
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
        // 分别描摹图 5-23 两侧连续的绿色道路带；保留前哨本体旁的凹口，
        // 只合并前哨与中央高地两段之间的接缝，不扩展到整个平台或强制镜像。
        {{747, 181}, {767, 181}, {767, 246}, {684, 367}, {684, 581},
         {723, 662}, {723, 707}, {690, 740}, {642, 740}, {667, 708},
         {676, 708}, {689, 696}, {689, 672}, {676, 659}, {658, 659},
         {634, 595}, {634, 348}},
        {{1006, 181}, {1054, 181}, {1026, 215}, {1008, 215}, {995, 227},
         {995, 251}, {1009, 264}, {1029, 264}, {1063, 325}, {1063, 568},
         {943, 740}, {923, 740}, {923, 672}, {1012, 555}, {1012, 342},
         {972, 262}, {972, 214}},
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
        transition.animation->setDuration(ui::TerritoryMotion);
        transition.animation->setStartValue(0.0);
        transition.animation->setEndValue(1.0);
        transition.animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(transition.animation, &QVariantAnimation::valueChanged, this, [this, index](const QVariant &value) {
            auto &t = m_transitions[index];
            const qreal progress = value.toReal();
            t.current = {blendColor(t.from.border, t.target.border, progress),
                         blendColor(t.from.band, t.target.band, progress),
                         t.from.dashed + (t.target.dashed - t.from.dashed) * progress,
                         t.from.slope + (t.target.slope - t.from.slope) * progress};
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
    // 光带白偏一点点暖：叠加后与底图纯白细描线自动分层，单看察觉不到。
    if (!state.value("known").toBool())
        return {QColor(ui::TerritoryUnknown), QColor(255, 250, 240, 58), 0, .65};
    const bool red = state.value("controller").toString() == "red";
    return {QColor(red ? ui::TeamRedFill : ui::TeamBlueFill), QColor(255, 250, 240, 98),
            state.value("temporary").toBool() ? 1.0 : 0.0, red ? .65 : -.65};
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
    const qreal spacing = qBound(9.0, width() * 0.016, 20.0);
    const qreal lineWidth = qBound(1.1, width() * 0.002, 2.2);
    constexpr qreal tau = 6.283185307179586;
    for (int index = 0; index < regionCount(); ++index) {
        const auto path = regionPath(index);
        const auto bounds = path.boundingRect();
        const auto state = regionState(index);
        const bool known = state.value("known").toBool();
        const auto &visual = m_transitions[regionTerritories[index]].current;
        // 红方用“\”、蓝方用“/”；方向来自过渡后的视觉状态，归属切换时随颜色一起翻转。
        const qreal slope = visual.slope;
        const qreal interceptSpacing = spacing * std::sqrt(1 + slope * slope);
        painter->fillPath(path, QColor(235, 248, 255, 7));
        painter->save();
        painter->setClipPath(path, Qt::IntersectClip);
        QPainterPath bands;
        // 光带在轮廓内连续流动，轻微正弦波动；周期接缝为整数行距，不闪回。
        // 截距范围取包围盒四角的极值：正负斜率（红蓝镜像）及过渡中的任意斜率都能铺满整个区域。
        const qreal lowest = bounds.top() + std::min(slope * bounds.left(), slope * bounds.right());
        const qreal highest = bounds.bottom() + std::max(slope * bounds.left(), slope * bounds.right());
        const qreal offset = std::fmod(m_phase * 6, 1.0) * interceptSpacing;
        const qreal first = std::floor(lowest / interceptSpacing) * interceptSpacing + offset - interceptSpacing;
        for (qreal intercept = first; intercept < highest + interceptSpacing; intercept += interceptSpacing) {
            bands.moveTo(bounds.left() - 6, intercept - slope * (bounds.left() - 6)
                + std::sin((bounds.left() - 6) * .025 + m_phase * tau) * 1.3);
            for (qreal x = bounds.left(); x <= bounds.right() + 12; x += 8)
                bands.lineTo(x, intercept - slope * x + std::sin(x * .025 + m_phase * tau) * 1.3);
        }
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(255, 255, 255, 26), lineWidth + 4)); painter->drawPath(bands);
        painter->setPen(QPen(QColor(255, 255, 255, 50), lineWidth + 1.5)); painter->drawPath(bands);
        painter->setPen(QPen(visual.band, lineWidth)); painter->drawPath(bands);
        painter->restore();
        // 加粗队色核心并向外逐层渐隐，保留柔光轮廓，不依赖整幅纹理模糊。
        painter->setBrush(Qt::NoBrush);
        for (const auto &stroke : {QPair<qreal, int>{9, 20}, {6, 45}, {3.8, 95}, {2.2, 230}}) {
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
            const QColor actor = advancing == "red" ? QColor(ui::TeamRed) : QColor(ui::TeamBlue);
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
                QFont font = painter->font(); font.setFamily(UiTheme::preferredTextFont()); font.setPixelSize(11); font.setBold(true); painter->setFont(font);
                const qreal labelWidth = QFontMetricsF(font).horizontalAdvance(text) + 14;
                QRectF box(center.x() - labelWidth / 2, center.y() - 11, labelWidth, 22);
                box.moveLeft(qBound(2.0, box.left(), width() - box.width() - 2));
                box.moveTop(qBound(2.0, box.top(), height() - box.height() - 2));
                // 切角标签与界面语言一致，不用圆角。
                const qreal cut = 3;
                QPainterPath tag;
                tag.moveTo(box.left() + cut, box.top());
                tag.lineTo(box.right(), box.top());
                tag.lineTo(box.right(), box.bottom() - cut);
                tag.lineTo(box.right() - cut, box.bottom());
                tag.lineTo(box.left(), box.bottom());
                tag.lineTo(box.left(), box.top() + cut);
                tag.closeSubpath();
                painter->setBrush(QColor(8, 16, 24, 235)); painter->setPen(QPen(actor, 1));
                painter->drawPath(tag);
                painter->setPen(actor); painter->drawText(box, Qt::AlignCenter, text);
            }
        }
    }
    painter->restore();
}

}
