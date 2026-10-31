#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace ly27 {

struct EncodedVideoFrame {
    quint16 frameId = 0;
    QByteArray bytes;
    qint64 receivedUtcMs = 0;
    qint64 receivedMonotonicMs = 0;
    bool discontinuity = false;
};

// 图像拥有独立内存；接收时间是该帧首个分片到达本机的时间
struct VideoFrame {
    QImage image;
    QString sessionId;
    quint16 frameId = 0;
    qint64 receivedUtcMs = 0;
    qint64 receivedMonotonicMs = 0;
    qint64 decodedMonotonicMs = 0;
};

} // 命名空间 ly27
