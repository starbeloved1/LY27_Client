#pragma once

#include <QImage>
#include <QQuickItem>
#include <QVariant>

namespace ly27 {

// 将接收器提供的最新图像上传为场景图纹理，不经过文件或图片编码。
class VideoSurface : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QVariant frame READ frame WRITE setFrame NOTIFY frameChanged)
public:
    explicit VideoSurface(QQuickItem *parent = nullptr);
    QVariant frame() const { return m_frame; }
    void setFrame(const QVariant &value);
signals:
    void frameChanged();
protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
private:
    QImage m_frame;
};

}
