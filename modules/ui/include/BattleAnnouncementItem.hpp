#pragma once

#include <QQuickPaintedItem>
#include <QVariantMap>

namespace ly27 {

// 复用 RM26 战场播报的绘制方式，不依赖 QWidget 或旧 GameData。
class BattleAnnouncementItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantMap notice READ notice WRITE setNotice NOTIFY noticeChanged)
public:
    explicit BattleAnnouncementItem(QQuickItem *parent = nullptr);
    QVariantMap notice() const { return m_notice; }
    void setNotice(const QVariantMap &value);
    void paint(QPainter *painter) override;
signals:
    void noticeChanged();
private:
    QVariantMap m_notice;
    void drawAvatar(QPainter &painter, const QRect &rect, int id, bool outpost);
};

}
