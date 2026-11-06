#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "BattleAnnouncementItem.hpp"
#include "TerritoryOverlayItem.hpp"

#include <QResource>
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
        if (id != "red_map_outpost.png" && id != "blue_map_outpost.png") return {};
        QImage image(QStringLiteral(":/ly27/ui/images/minimap/") + id);
        image = image.convertToFormat(QImage::Format_ARGB32);
        // 在保留原图轮廓与透明度的基础上提亮、提高色彩饱和度，兼容软件渲染。
        for (int y = 0; y < image.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = QColor::fromRgba(line[x]);
                if (pixel.alpha() == 0) continue;
                line[x] = QColor::fromHsvF(pixel.hsvHueF(), qMin(1.0f, pixel.hsvSaturationF() * 1.35f),
                    qMin(1.0f, pixel.valueF() * 1.18f), pixel.alphaF()).rgba();
            }
        }
        if (size) *size = image.size();
        return image;
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

MapWindow::MapWindow(TerminalViewModel *model, QWindow *parent)
    : QQuickView(parent), m_tactics(model ? model->tactics() : nullptr)
{
    initializeUiResources();
    static const int announcementType = qmlRegisterType<BattleAnnouncementItem>("LY27.Ui", 1, 0, "BattleAnnouncementItem");
    Q_UNUSED(announcementType);
    static const int territoryType = qmlRegisterType<TerritoryOverlayItem>("LY27.Ui", 1, 0, "TerritoryOverlayItem");
    Q_UNUSED(territoryType);
    static const int tacticsType = qmlRegisterUncreatableType<TacticalMapController>("LY27.Ui", 1, 0,
        "TacticalMapController", QStringLiteral("由终端展示适配持有"));
    Q_UNUSED(tacticsType);
    setTitle(QStringLiteral("LY27 Client"));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumSize(QSize(640, 480));
    resize(1280, 800);
    rootContext()->setContextProperty(QStringLiteral("terminal"), model);
    rootContext()->setContextProperty(QStringLiteral("desktopWindow"), this);
    engine()->addImageProvider(QStringLiteral("outpost"), new OutpostImageProvider);
    engine()->addImageProvider(QStringLiteral("buff"), new BuffImageProvider);
    setSource(QUrl(QStringLiteral("qrc:/ly27/ui/Map.qml")));
}

void MapWindow::setInjuryPanelVisible(bool visible)
{
    if (m_injuryPanelVisible == visible) return;
    m_injuryPanelVisible = visible;
    emit injuryPanelVisibleChanged();
}

void MapWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_tactics && m_tactics->commander()) {
        m_tactics->cancel(); event->accept(); return;
    }
    auto *focus = activeFocusItem();
    const bool editing = focus && (focus->inherits("QQuickTextInput") || focus->inherits("QQuickTextEdit"));
    if (injuryHotkey(event) && !editing &&
        !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        if (!event->isAutoRepeat()) setInjuryPanelVisible(true);
        event->accept();
        return;
    }
    QQuickView::keyPressEvent(event);
}

void MapWindow::keyReleaseEvent(QKeyEvent *event)
{
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
        event->type() == QEvent::Hide || event->type() == QEvent::Close) setInjuryPanelVisible(false);
    return QQuickView::event(event);
}

}
