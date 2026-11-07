#include "MapWindow.hpp"
#include "UiTheme.hpp"
#include "TerminalViewModel.hpp"
#include "DesktopSession.hpp"
#include "BattleAnnouncementItem.hpp"
#include "TerritoryOverlayItem.hpp"
#include "VideoSurface.hpp"

#include <QResource>
#include <QPainter>
#include <array>
#include <cmath>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickImageProvider>
#include <QQmlEngine>
#include <QUrl>
#include <QQmlContext>
#include <qqml.h>

static void initializeUiResources()
{
    // 显式引用静态库资源，避免链接器将未引用的资源对象移除。
    Q_INIT_RESOURCE(ly27_ui_resources);
}

namespace ly27 {
namespace {
// 底图的色调只计算一次，各窗口/缩放共用缓存；不依赖 GPU 特效或修改原素材。
class BattlefieldImageProvider final : public QQuickImageProvider {
public:
    BattlefieldImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        if (id != "map") return {};
        static const QImage toned = [] {
            QImage image(":/ly27/ui/maps/battlefield.png");
            image = image.convertToFormat(QImage::Format_ARGB32);
            std::array<int, 256> tone{};
            for (int i = 0; i < 256; ++i)
                tone[i] = qBound(0, qRound(((i - 128) * 1.06 + 128) * .80), 255);
            // 径向暗角：中心不衰减、四角压暗，视线收到战场中央；周边装饰自动退后。
            const qreal w = image.width(), h = image.height();
            const qreal cx = w * .5, cy = h * .48;
            const qreal maxRadius = std::hypot(w * .5, h * .5);
            constexpr qreal inner = .42;   // 该半径内完全不衰减
            constexpr qreal outer = .78;   // 该半径处衰减最强
            constexpr qreal strength = .34;
            for (int y = 0; y < image.height(); ++y) {
                auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    const auto pixel = line[x];
                    const int r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);
                    const int luminance = (r * 54 + g * 183 + b * 19 + 128) >> 8;
                    const auto channel = [&](int value) { return tone[(value * 55 + luminance * 45 + 50) / 100]; };
                    // 二次缓出，边缘过渡柔和，缩略图上不出现可见的圈。
                    const qreal distance = std::hypot(x - cx, y - cy) / maxRadius;
                    const qreal t = qBound(0.0, (distance - inner) / (outer - inner), 1.0);
                    const qreal falloff = 1.0 - strength * t * t;
                    line[x] = qRgba(int(channel(r) * falloff), int(channel(g) * falloff),
                                    int(channel(b) * falloff), qAlpha(pixel));
                }
            }
            return image;
        }();
        if (size) *size = toned.size();
        return toned;
    }
};
class BuffImageProvider final : public QQuickImageProvider {
public:
    BuffImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        static const QMap<QString, QPair<QString, qreal>> assets{
            {"attack", {"attack_300.png", .65}}, {"defense", {"defence_100.png", .65}},
            {"vulnerability", {"debuff_100.png", .65}}, {"cooling", {"cool_up.png", .65}},
            {"recovery", {"addhp_25.png", .65}}, {"power", {"terrain_fly_green.png", 1}},
            {"ammo", {"ammo.png", 1}}, {"terrain", {"terrain_highground_green.png", 1}}
        };
        if (!assets.contains(id)) return {};
        const auto asset = assets.value(id);
        QImage source(":/ly27/ui/images/buffs/" + asset.first);
        // 复用原素材的符号部分，裁去 26 的固定倍率文字，避免冒充 27 实际增益值。
        source = source.copy(0, 0, source.width(), qRound(source.height() * asset.second));
        QRect content;
        for (int y = 0; y < source.height(); ++y) for (int x = 0; x < source.width(); ++x)
            if (source.pixelColor(x, y).alpha() > 12) content |= QRect(x, y, 1, 1);
        if (!content.isEmpty()) source = source.copy(content);
        if (size) *size = source.size();
        return source;
    }
};
class OutpostImageProvider final : public QQuickImageProvider {
public:
    OutpostImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        if (id != "red_map_outpost.png" && id != "blue_map_outpost.png" && id != "unknown_map_outpost.png") return {};
        QImage image(QStringLiteral(":/ly27/ui/images/minimap/") + (id == "unknown_map_outpost.png" ? "red_map_outpost.png" : id));
        image = image.convertToFormat(QImage::Format_ARGB32);
        QImage glow(image.size(), QImage::Format_ARGB32_Premultiplied);
        glow.fill(Qt::transparent);
        // 原素材的渐隐柔光与清晰主体分层保留，避免锐化时把外发光一起截掉。
        for (int y = 0; y < image.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = QColor::fromRgba(line[x]);
                if (pixel.alpha() == 0) continue;
                const auto tinted = QColor::fromHsvF(pixel.hsvHueF(), qMin(1.0f, pixel.hsvSaturationF() * 1.55f),
                    pixel.valueF() * .84f, qMin(1.0f, pixel.alphaF() * 1.15f));
                glow.setPixelColor(x, y, tinted);
                const qreal edge = qBound(0.0, (pixel.alpha() - 18) / 72.0, 1.0);
                const qreal alpha = edge * edge * (3 - 2 * edge);
                if (pixel.alpha() >= 140 && pixel.hsvSaturationF() < .4f) {
                    QColor symbol(ui::HeadingInk); symbol.setAlphaF(alpha); line[x] = symbol.rgba();
                } else {
                    line[x] = QColor::fromHsvF(pixel.hsvHueF(), qMin(1.0f, pixel.hsvSaturationF() * 1.55f),
                        pixel.valueF() * .84f, alpha).rgba();
                }
            }
        }
        // 仅沿符号自身轮廓外扩一个素材像素，不增加圆环或独立底板。
        QImage outlined(image.size(), QImage::Format_ARGB32_Premultiplied);
        outlined.fill(Qt::transparent);
        for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
            int alpha = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                if (x + dx < 0 || x + dx >= image.width() || y + dy < 0 || y + dy >= image.height()) continue;
                alpha = qMax(alpha, qAlpha(image.pixel(x + dx, y + dy)));
            }
            outlined.setPixelColor(x, y, QColor(9, 16, 23, qRound(alpha * .55)));
        }
        QPainter painter(&glow);
        painter.drawImage(0, 0, outlined);
        painter.drawImage(0, 0, image);
        painter.end();
        if (id == "unknown_map_outpost.png") {
            glow = glow.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < glow.height(); ++y) for (int x = 0; x < glow.width(); ++x) {
                const QRgb pixel = glow.pixel(x, y);
                const int gray = qGray(pixel);
                glow.setPixel(x, y, qRgba(gray, gray, gray, qAlpha(pixel)));
            }
        }
        if (size) *size = image.size();
        return glow;
    }
};

// 与 RM26 的伤害面板按键习惯一致，兼容 Shift、死键和 Windows OEM 键。
bool injuryHotkey(const QKeyEvent *event)
{
    if (event->key() == Qt::Key_QuoteLeft || event->key() == Qt::Key_AsciiTilde ||
        event->key() == Qt::Key_Dead_Grave || event->text().contains(QLatin1Char('`')) ||
        event->text().contains(QLatin1Char('~'))) return true;
#ifdef Q_OS_WIN
    return event->nativeVirtualKey() == 0xC0 || event->nativeScanCode() == 0x29;
#else
    return false;
#endif
}
}

MapWindow::MapWindow(TerminalViewModel *model, QWindow *parent, DesktopSession *session)
    : QQuickView(parent), m_tactics(model ? model->tactics() : nullptr)
{
    initializeUiResources();
    static const int themeType = qmlRegisterSingletonType<UiTheme>("LY27.Ui", 1, 0, "Theme",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new UiTheme; });
    Q_UNUSED(themeType);
    static const int announcementType = qmlRegisterType<BattleAnnouncementItem>("LY27.Ui", 1, 0, "BattleAnnouncementItem");
    Q_UNUSED(announcementType);
    static const int territoryType = qmlRegisterType<TerritoryOverlayItem>("LY27.Ui", 1, 0, "TerritoryOverlayItem");
    Q_UNUSED(territoryType);
    static const int videoType = qmlRegisterType<VideoSurface>("LY27.Ui", 1, 0, "VideoSurface");
    Q_UNUSED(videoType);
    static const int tacticsType = qmlRegisterUncreatableType<TacticalMapController>("LY27.Ui", 1, 0,
        "TacticalMapController", QStringLiteral("由终端展示适配持有"));
    Q_UNUSED(tacticsType);
    setTitle(QStringLiteral("LY27 Client"));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumSize(QSize(640, 480));
    resize(1280, 800);
    connect(this, &QWindow::visibilityChanged, this, [this] { emit fullScreenChanged(); });
    rootContext()->setContextProperty(QStringLiteral("terminal"), model);
    rootContext()->setContextProperty(QStringLiteral("desktopWindow"), this);
    rootContext()->setContextProperty(QStringLiteral("sessionSettings"), static_cast<QObject *>(session));
    if (session) {
        setTitle(session->windowTitle());
        connect(session, &DesktopSession::viewModelChanged, this, [this, session] {
            setInjuryPanelVisible(false);
            m_tactics = session->viewModel()->tactics();
            rootContext()->setContextProperty(QStringLiteral("terminal"), session->viewModel());
            setTitle(session->windowTitle());
        });
    }
    engine()->addImageProvider(QStringLiteral("battlefield"), new BattlefieldImageProvider);
    engine()->addImageProvider(QStringLiteral("outpost"), new OutpostImageProvider);
    engine()->addImageProvider(QStringLiteral("buff"), new BuffImageProvider);
    setSource(QUrl(QStringLiteral("qrc:/ly27/ui/Map.qml")));
}

void MapWindow::setFullScreen(bool enabled)
{
    if (enabled == fullScreen()) return;
    if (enabled) {
        m_windowedVisibility = visibility() == QWindow::Maximized ? QWindow::Maximized : QWindow::Windowed;
        m_windowedGeometry = geometry();
        showFullScreen();
    } else if (m_windowedVisibility == QWindow::Maximized) {
        showMaximized();
    } else {
        showNormal();
        if (m_windowedGeometry.isValid()) setGeometry(m_windowedGeometry);
    }
}

void MapWindow::setInjuryPanelVisible(bool visible)
{
    if (m_injuryPanelVisible == visible) return;
    m_injuryPanelVisible = visible;
    emit injuryPanelVisibleChanged();
}

void MapWindow::setSettingsVisible(bool visible)
{
    if (m_settingsVisible == visible) return;
    m_settingsVisible = visible;
    if (visible) setInjuryPanelVisible(false);
    emit settingsVisibleChanged();
}

void MapWindow::keyPressEvent(QKeyEvent *event)
{
    auto *focus = activeFocusItem();
    const bool editing = focus && (focus->inherits("QQuickTextInput") || focus->inherits("QQuickTextEdit"));
    const bool plainKey = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (!editing && plainKey && event->key() == Qt::Key_Space) {
        if (!event->isAutoRepeat()) setSettingsVisible(true);
        event->accept(); return;
    }
    if (!editing && plainKey && event->key() == Qt::Key_P) {
        if (!event->isAutoRepeat()) {
            if (m_tactics) m_tactics->cancel();
            m_videoVisible = !m_videoVisible;
            emit videoVisibleChanged();
        }
        event->accept(); return;
    }
    if (m_settingsVisible) {
        QQuickView::keyPressEvent(event); return;
    }
    if (event->key() == Qt::Key_Escape && m_tactics && m_tactics->commander()) {
        m_tactics->cancel(); event->accept(); return;
    }
    if (injuryHotkey(event) && !editing && plainKey) {
        if (!event->isAutoRepeat()) setInjuryPanelVisible(true);
        event->accept();
        return;
    }
    QQuickView::keyPressEvent(event);
}

void MapWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && m_settingsVisible) {
        if (!event->isAutoRepeat()) setSettingsVisible(false);
        event->accept(); return;
    }
    if (injuryHotkey(event) && m_injuryPanelVisible) {
        if (!event->isAutoRepeat()) setInjuryPanelVisible(false);
        event->accept();
        return;
    }
    QQuickView::keyReleaseEvent(event);
}

bool MapWindow::event(QEvent *event)
{
    // 切到别的窗口后可能收不到松键事件，主动收起，防止面板常驻。
    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::FocusOut ||
        event->type() == QEvent::Hide || event->type() == QEvent::Close) {
        setInjuryPanelVisible(false);
        setSettingsVisible(false);
    }
    return QQuickView::event(event);
}

}
