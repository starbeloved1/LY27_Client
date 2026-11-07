#include "VideoSurface.hpp"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <memory>

namespace ly27 {
namespace {
// 纹理只在渲染线程创建和释放；替换时不将历史帧排入绘制队列。
class VideoNode final : public QSGSimpleTextureNode {
public:
    std::unique_ptr<QSGTexture> ownedTexture;
    qint64 imageKey = 0;
};
}

VideoSurface::VideoSurface(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void VideoSurface::setFrame(const QVariant &value)
{
    const QImage image = value.value<QImage>();
    if (image.cacheKey() == m_frame.cacheKey()) return;
    m_frame = image;
    emit frameChanged();
    update();
}

void VideoSurface::geometryChange(const QRectF &next, const QRectF &previous)
{
    QQuickItem::geometryChange(next, previous);
    if (next.size() != previous.size()) update();
}

QSGNode *VideoSurface::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (m_frame.isNull() || !window() || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }
    auto *node = static_cast<VideoNode *>(oldNode);
    if (!node) node = new VideoNode;
    if (node->imageKey != m_frame.cacheKey()) {
        std::unique_ptr<QSGTexture> texture(window()->createTextureFromImage(m_frame));
        if (!texture) { delete node; return nullptr; }
        node->setTexture(texture.get());
        node->ownedTexture = std::move(texture);
        node->imageKey = m_frame.cacheKey();
        node->setFiltering(QSGTexture::Linear);
    }
    const QSizeF fitted = QSizeF(m_frame.size()).scaled(boundingRect().size(), Qt::KeepAspectRatio);
    node->setRect(QRectF(QPointF((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted));
    return node;
}

}
