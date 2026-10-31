#pragma once

#include "VideoTypes.hpp"
#include <QJsonObject>
#include <QMap>
#include <optional>

// 校验包头与重组分片，清理不完整帧
namespace ly27 {

class VideoAssembler {
public:
    static constexpr int MaxDatagramBytes = 1400;
    static constexpr int MaxFrameBytes = 4 * 1024 * 1024;
    static constexpr int MaxPendingFrames = 8;
    static constexpr int MaxBufferedBytes = 16 * 1024 * 1024;
    static constexpr int MaxFragments = 4096;
    static constexpr int TimeoutMs = 150;
    void reset();
    void expire(qint64 nowMs);
    std::optional<EncodedVideoFrame> push(const QByteArray &packet, qint64 nowMs, qint64 utcMs);
    QJsonObject snapshot() const;
    qint64 invalidDatagrams() const { return m_invalid; }

private:
    struct Pending {
        quint32 totalBytes = 0;
        int bytes = 0;
        qint64 firstMs = 0;
        qint64 utcMs = 0;
        QMap<quint16, QByteArray> chunks;
    };
    QMap<quint16, Pending> m_pending;
    std::optional<quint16> m_lastFrame;
    qint64 m_lastCompleteMs = -1;
    int m_buffered = 0;
    qint64 m_packets = 0, m_invalid = 0, m_duplicates = 0, m_dropped = 0, m_completed = 0;
    void drop(quint16 id);
};

} // 命名空间 ly27
