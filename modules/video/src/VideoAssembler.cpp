#include "VideoAssembler.hpp"
#include <QtEndian>
#include <algorithm>

namespace ly27 {
void VideoAssembler::reset() { *this = VideoAssembler{}; }

void VideoAssembler::drop(quint16 id)
{
    const auto it = m_pending.find(id);
    if (it == m_pending.end()) return;
    m_buffered -= it->bytes;
    m_pending.erase(it);
    ++m_dropped;
}

void VideoAssembler::expire(qint64 nowMs)
{
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (nowMs - it->firstMs >= TimeoutMs) {
            m_buffered -= it->bytes;
            it = m_pending.erase(it);
            ++m_dropped;
        } else ++it;
    }
    // 长时间停流后允许发送端从新的帧号重新开始。
    if (m_lastCompleteMs >= 0 && nowMs - m_lastCompleteMs >= 1000) m_lastFrame.reset();
}

std::optional<EncodedVideoFrame> VideoAssembler::push(const QByteArray &packet, qint64 nowMs, qint64 utcMs)
{
    ++m_packets;
    expire(nowMs);
    if (packet.size() < 9 || packet.size() > MaxDatagramBytes) { ++m_invalid; return {}; }
    const auto *data = reinterpret_cast<const uchar *>(packet.constData());
    const auto id = qFromBigEndian<quint16>(data);
    const auto index = qFromBigEndian<quint16>(data + 2);
    const auto total = qFromBigEndian<quint32>(data + 4);
    const auto chunk = packet.mid(8);
    if (!total || total > MaxFrameBytes || index >= MaxFragments || chunk.size() > total) {
        ++m_invalid; return {};
    }
    if (m_lastFrame) {
        const auto distance = static_cast<quint16>(id - *m_lastFrame);
        if (!distance || distance >= 0x8000) { ++m_duplicates; return {}; }
    }
    if (!m_pending.contains(id)) {
        while (m_pending.size() >= MaxPendingFrames) {
            auto oldest = std::min_element(m_pending.begin(), m_pending.end(),
                [](const Pending &a, const Pending &b) { return a.firstMs < b.firstMs; });
            drop(oldest.key());
        }
        Pending pending;
        pending.totalBytes = total;
        pending.firstMs = nowMs;
        pending.utcMs = utcMs;
        m_pending.insert(id, std::move(pending));
    }
    auto &pending = m_pending[id];
    if (pending.totalBytes != total) { ++m_invalid; drop(id); return {}; }
    if (pending.chunks.contains(index)) {
        if (pending.chunks[index] == chunk) ++m_duplicates;
        else { ++m_invalid; drop(id); }
        return {};
    }
    if (pending.bytes + chunk.size() > total || m_buffered + chunk.size() > MaxBufferedBytes) {
        ++m_invalid; drop(id); return {};
    }
    pending.chunks.insert(index, chunk);
    pending.bytes += chunk.size();
    m_buffered += chunk.size();
    if (pending.bytes != static_cast<int>(total)) return {};
    // 文档未定义分片序号起点；兼容从 0 或 1 开始，但必须连续且总长度严格相等。
    int expected = pending.chunks.firstKey();
    if (expected > 1) { ++m_invalid; drop(id); return {}; }
    for (auto it = pending.chunks.cbegin(); it != pending.chunks.cend(); ++it) {
        if (it.key() != expected++) { ++m_invalid; drop(id); return {}; }
    }
    EncodedVideoFrame result;
    result.frameId = id;
    result.receivedUtcMs = pending.utcMs;
    result.receivedMonotonicMs = pending.firstMs;
    result.discontinuity = !m_lastFrame || static_cast<quint16>(id - *m_lastFrame) != 1;
    result.bytes.reserve(static_cast<int>(total));
    for (const auto &bytes : pending.chunks) result.bytes.append(bytes);
    m_buffered -= pending.bytes;
    m_pending.remove(id);
    m_lastFrame = id;
    m_lastCompleteMs = nowMs;
    ++m_completed;
    return result;
}

QJsonObject VideoAssembler::snapshot() const
{
    return {{"datagrams", m_packets}, {"invalid_datagrams", m_invalid}, {"duplicate_or_old_datagrams", m_duplicates},
        {"dropped_frames", m_dropped}, {"assembled_frames", m_completed},
        {"pending_frames", m_pending.size()}, {"buffered_bytes", m_buffered}};
}
} // 命名空间 ly27
