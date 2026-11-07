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
    static const auto family = availableFont({"JetBrains Mono", "DejaVu Sans Mono", "Consolas", "Menlo"},
        QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    return family;
}
UiTheme::UiTheme(QObject *parent)
    : QObject(parent), textFont(preferredTextFont()), numberFont(preferredNumberFont())
{}

}
