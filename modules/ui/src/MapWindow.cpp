#include "MapWindow.hpp"
#include "TerminalViewModel.hpp"
#include "BattleAnnouncementItem.hpp"

#include <QResource>
#include <QUrl>
#include <QQmlContext>
#include <qqml.h>

static void initializeUiResources()
{
    // 显式引用静态库资源，避免链接器将未引用的资源对象移除。
    Q_INIT_RESOURCE(ly27_ui_resources);
}

namespace ly27 {

MapWindow::MapWindow(TerminalViewModel *model, QWindow *parent)
    : QQuickView(parent)
{
    initializeUiResources();
    static const int announcementType = qmlRegisterType<BattleAnnouncementItem>("LY27.Ui", 1, 0, "BattleAnnouncementItem");
    Q_UNUSED(announcementType);
    setTitle(QStringLiteral("LY27 Client"));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumSize(QSize(640, 480));
    resize(1280, 800);
    rootContext()->setContextProperty(QStringLiteral("terminal"), model);
    setSource(QUrl(QStringLiteral("qrc:/ly27/ui/Map.qml")));
}

}
