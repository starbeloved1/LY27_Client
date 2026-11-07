#include "UiTheme.hpp"

#include <QFontDatabase>
#include <QGuiApplication>

namespace ly27 {
namespace {
QString availableFont(const QStringList &preferred, const QString &fallback)
{
    const auto families = QFontDatabase::families();
    for (const auto &family : preferred)
        if (families.contains(family, Qt::CaseInsensitive)) return family;
    return fallback;
}
}
QString UiTheme::preferredTextFont()
{
    static const auto family = availableFont({"Noto Sans CJK SC", "Noto Sans SC", "Source Han Sans SC", "Microsoft YaHei", "PingFang SC"},
        QGuiApplication::font().family());
    return family;
}
QString UiTheme::preferredNumberFont()
{
    // 窄体无衬线：0 不带斜杠，相同宽度下能放更大字号，读数更像比分而不是终端日志。
    static const auto family = availableFont({
        "Liberation Sans Narrow", "Nimbus Sans Narrow", "Ubuntu Condensed",
        "DejaVu Sans Condensed", "Arial Narrow", "Roboto Condensed"},
        QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    return family;
}
UiTheme::UiTheme(QObject *parent)
    : QObject(parent), textFont(preferredTextFont()), numberFont(preferredNumberFont())
{}

}
