#pragma once

#include <QColor>
#include <QObject>
#include <QString>

namespace ly27 {

// QML 与 C++ 绘图共用语义色；阵营、告警与指挥意图各自保持固定含义。
namespace ui {
inline constexpr auto TeamRed = "#FF737A";
inline constexpr auto TeamBlue = "#62C9FF";
// 深色用于有体积的填充，亮色仍用于文字、提示和推进高光。
inline constexpr auto TeamRedFill = "#DE3A47";
inline constexpr auto TeamBlueFill = "#1B85CC";
inline constexpr auto SelfFill = "#269F76";
inline constexpr auto HeadingInk = "#F2F6FA";
inline constexpr auto Positive = "#60D8B7";
inline constexpr auto Unknown = "#71818F";
inline constexpr auto TerritoryUnknown = "#9AA8B4";
inline constexpr auto Text = "#E9EEF2";
inline constexpr auto Secondary = "#9BAEBC";
inline constexpr auto Tactical = "#F2CF7F";
inline constexpr int NumericMotion = 240;
inline constexpr int TerritoryMotion = 600;
}

// 每个 QML 引擎持有自己的只读主题，字体从本机可用字体中选择，不依赖外部下载。
class UiTheme final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor background MEMBER background CONSTANT)
    Q_PROPERTY(QColor panel MEMBER panel CONSTANT)
    Q_PROPERTY(QColor raised MEMBER raised CONSTANT)
    Q_PROPERTY(QColor inset MEMBER inset CONSTANT)
    Q_PROPERTY(QColor border MEMBER border CONSTANT)
    Q_PROPERTY(QColor divider MEMBER divider CONSTANT)
    Q_PROPERTY(QColor text MEMBER text CONSTANT)
    Q_PROPERTY(QColor secondary MEMBER secondary CONSTANT)
    Q_PROPERTY(QColor muted MEMBER muted CONSTANT)
    Q_PROPERTY(QColor unknown MEMBER unknown CONSTANT)
    Q_PROPERTY(QColor teamRed MEMBER teamRed CONSTANT)
    Q_PROPERTY(QColor teamBlue MEMBER teamBlue CONSTANT)
    Q_PROPERTY(QColor selfGreen MEMBER selfGreen CONSTANT)
    Q_PROPERTY(QColor teamRedFill MEMBER teamRedFill CONSTANT)
    Q_PROPERTY(QColor teamBlueFill MEMBER teamBlueFill CONSTANT)
    Q_PROPERTY(QColor selfFill MEMBER selfFill CONSTANT)
    Q_PROPERTY(QColor headingInk MEMBER headingInk CONSTANT)
    Q_PROPERTY(QColor positive MEMBER positive CONSTANT)
    Q_PROPERTY(QColor danger MEMBER danger CONSTANT)
    Q_PROPERTY(QColor tactical MEMBER tactical CONSTANT)
    Q_PROPERTY(QColor overlay MEMBER overlay CONSTANT)
    Q_PROPERTY(QColor ink MEMBER ink CONSTANT)
    Q_PROPERTY(QString textFont MEMBER textFont CONSTANT)
    Q_PROPERTY(QString numberFont MEMBER numberFont CONSTANT)
    Q_PROPERTY(int caption MEMBER caption CONSTANT)
    Q_PROPERTY(int body MEMBER body CONSTANT)
    Q_PROPERTY(int heading MEMBER heading CONSTANT)
    Q_PROPERTY(int number MEMBER number CONSTANT)
    Q_PROPERTY(int padding MEMBER padding CONSTANT)
    Q_PROPERTY(int gap MEMBER gap CONSTANT)
    Q_PROPERTY(int quickMotion MEMBER quickMotion CONSTANT)
    Q_PROPERTY(int numericMotion MEMBER numericMotion CONSTANT)
    Q_PROPERTY(int healthMotion MEMBER healthMotion CONSTANT)
public:
    explicit UiTheme(QObject *parent = nullptr);
    static QString preferredTextFont();
    static QString preferredNumberFont();
    const QColor background{"#0B0F14"}, panel{"#121820"}, raised{"#19212A"}, inset{"#0D131A"};
    const QColor border{"#34414C"}, divider{"#26323D"};
    const QColor text{ui::Text}, secondary{ui::Secondary}, muted{"#6F8292"}, unknown{ui::Unknown};
    const QColor teamRed{ui::TeamRed}, teamBlue{ui::TeamBlue}, selfGreen{"#7BE4B0"};
    const QColor teamRedFill{ui::TeamRedFill}, teamBlueFill{ui::TeamBlueFill}, selfFill{ui::SelfFill}, headingInk{ui::HeadingInk};
    const QColor positive{ui::Positive}, danger{ui::TeamRed}, tactical{ui::Tactical};
    const QColor overlay{"#F010171F"}, ink{"#091017"};
    const QString textFont, numberFont;
    const int caption = 11, body = 13, heading = 17, number = 28;
    const int padding = 12, gap = 8, quickMotion = 80, numericMotion = ui::NumericMotion, healthMotion = 160;
};

}
