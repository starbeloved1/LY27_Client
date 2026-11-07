#include "UiTheme.hpp"
#include "BattleAnnouncementItem.hpp"

#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

namespace ly27 {
namespace {
QImage asset(const QString &path)
{
    return QImage(QStringLiteral(":/ly27/ui/images/") + path);
}
}

BattleAnnouncementItem::BattleAnnouncementItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::NoButton);
}
void BattleAnnouncementItem::setNotice(const QVariantMap &value)
{
    if (m_notice == value) return;
    m_notice = value; update(); emit noticeChanged();
}
void BattleAnnouncementItem::drawAvatar(QPainter &p, const QRect &rect, int id, bool outpost)
{
    p.save();
    const bool red = id < 100;
    const QColor team = red ? QColor(ui::TeamRed) : QColor(ui::TeamBlue);
    const QString color = red ? QStringLiteral("red") : QStringLiteral("blue");
    p.setPen(QPen(team, 2)); p.setBrush(QColor(0, 0, 0, 180)); p.drawEllipse(rect);
    const int number = id % 100;
    QString type;
    switch (number) {
    case 1: type = "hero"; break;
    case 3: case 4: type = "infantry"; break;
    case 6: type = "drone"; break;
    case 7: type = "sentry"; break;
    default: break;
    }
    const QImage image = outpost ? asset("message/" + color + "_message_outpost.png")
        : type.isEmpty() ? QImage{} : asset("robots/" + color + "_" + type + ".png");
    const QRect inner = rect.adjusted(4, 4, -4, -4);
    if (!image.isNull()) {
        if (!outpost) {
            QPainterPath mask; mask.addEllipse(inner); p.setClipPath(mask);
        }
        p.drawImage(inner, image);
        p.setClipping(false);
    } else {
        p.setPen(Qt::white); p.drawText(inner, Qt::AlignCenter, QString::number(number));
    }
    if (!outpost) {
        const int size = rect.width() / 3;
        const QRect badge(rect.right() - size, rect.bottom() - size, size, size);
        p.setPen(QPen(Qt::white, 1)); p.setBrush(team); p.drawEllipse(badge);
        QFont font = p.font(); font.setPixelSize(size * 0.65); font.setBold(true); p.setFont(font);
        p.drawText(badge, Qt::AlignCenter, QString::number(number));
    }
    p.restore();
}
void BattleAnnouncementItem::paint(QPainter *painter)
{
    if (m_notice.isEmpty()) return;
    auto &p = *painter;
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const bool outpost = m_notice.value("kind").toString() == "outpost";
    const int killer = m_notice.value("killer").toInt();
    const int victim = m_notice.value("victim").toInt();
    const QString text = m_notice.value("text").toString();
    QFont font("Roboto"); font.setPixelSize(16); font.setBold(true); p.setFont(font);
    const int avatarSize = 40, padding = 10, spacing = 5;
    const int textWidth = QFontMetrics(font).horizontalAdvance(text) + padding * 2;
    const int totalWidth = avatarSize * (killer ? 2 : 1) + textWidth + spacing * (killer ? 2 : 1) + padding * 2;
    const int x = (width() - totalWidth) / 2;
    const int y = (height() - 50) / 2;
    const QRect background(x, y, totalWidth, 50);
    // 底框、头像、圆形编号及并排构图取自 RM26 BattleMessageWidget。
    const bool red = m_notice.value("red").toBool();
    p.drawImage(background, asset(red ? "message/message_red.png" : "message/message_blue.png"));
    const QRect left(x + padding, y + 5, avatarSize, avatarSize);
    drawAvatar(p, left, killer ? killer : victim, outpost);
    const QRect words(left.x() + avatarSize + spacing, y, textWidth, 50);
    p.setPen(Qt::white); p.drawText(words, Qt::AlignCenter, text);
    if (killer) drawAvatar(p, QRect(words.x() + textWidth + spacing, y + 5, avatarSize, avatarSize), victim, false);
}

}
