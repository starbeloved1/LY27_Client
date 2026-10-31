#include "VideoReceiver.hpp"
#include "HevcDecoder.hpp"
#include "VideoAssembler.hpp"
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkDatagram>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <chrono>
#include <deque>

namespace ly27 {
namespace {
qint64 monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Mailbox {
    QMutex mutex;
    QJsonObject stats;
    std::optional<VideoFrame> latest;
    quint64 revision = 0;
};

// 网络、重组和解码均在专用线程；跨线程只共享一帧和低频统计，不排队发送像素数据。
class VideoWorker : public QObject {
public:
    explicit VideoWorker(std::shared_ptr<Mailbox> mailbox, QString session)
        : box(std::move(mailbox)), sessionId(std::move(session)) {}

    bool start(quint16 port, const QHostAddress &bind, const QHostAddress &source, QString &error)
    {
        expectedSource = source;
        if (!decoder.start(error)) return false;
        socket = new QUdpSocket(this);
        if (!socket->bind(bind, port, QUdpSocket::DontShareAddress)) {
            error = socket->errorString(); return false;
        }
        socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 4 * 1024 * 1024);
        connect(socket, &QUdpSocket::readyRead, this, [this] { read(); });
        maintenance = new QTimer(this);
        maintenance->setInterval(50);
        connect(maintenance, &QTimer::timeout, this, [this] {
            const auto now = monotonicMs();
            assembler.expire(now);
            if (lastPacketMs >= 0 && now - lastPacketMs >= 1000 && !idleReset) {
                queue.clear(); decoder.reset(); peer = {}; idleReset = true;
            }
            update();
        });
        boundPort = socket->localPort();
        maintenance->start();
        update();
        return true;
    }

    void stop()
    {
        if (maintenance) maintenance->stop();
        if (socket) socket->close();
        stopping = true;
        queue.clear();
        update();
    }

private:
    std::shared_ptr<Mailbox> box;
    QString sessionId;
    QUdpSocket *socket = nullptr;
    QTimer *maintenance = nullptr;
    QHostAddress expectedSource, peer;
    quint16 peerPort = 0, boundPort = 0;
    VideoAssembler assembler;
    HevcDecoder decoder;
    std::deque<EncodedVideoFrame> queue;
    bool readScheduled = false;
    bool scheduled = false, stopping = false, idleReset = true;
    qint64 lastPacketMs = -1;
    qint64 frames = 0, errors = 0, overflows = 0, foreign = 0;
    QString lastError;

    void update()
    {
        auto stats = assembler.snapshot();
        stats.insert("port", boundPort);
        stats.insert("decoded_frames", frames);
        stats.insert("decode_errors", errors);
        stats.insert("queue_overflows", overflows);
        stats.insert("ignored_source_datagrams", foreign);
        stats.insert("waiting_keyframe", decoder.waitingForKeyframe());
        stats.insert("last_error", lastError);
        stats.insert("queued_frames", static_cast<qint64>(queue.size()));
        stats.insert("peer_address", peer.toString());
        QMutexLocker lock(&box->mutex);
        box->stats = stats;
    }

    void read()
    {
        // 每轮读取有上限，给解码和停止事件留出调度机会。
        int budget = 256;
        while (!stopping && socket->hasPendingDatagrams() && budget-- > 0) {
            const auto datagram = socket->receiveDatagram(1401);
            const auto address = datagram.senderAddress();
            const auto port = static_cast<quint16>(datagram.senderPort());
            if ((!expectedSource.isNull() && address != expectedSource)
                || (!peer.isNull() && (address != peer || port != peerPort))) {
                ++foreign; continue;
            }
            const auto now = monotonicMs();
            const auto before = assembler.invalidDatagrams();
            auto frame = assembler.push(datagram.data(), now, QDateTime::currentMSecsSinceEpoch());
            if (assembler.invalidDatagrams() != before) continue;
            peer = address; peerPort = port; lastPacketMs = now; idleReset = false;
            if (frame) {
                if (queue.size() >= 4) {
                    ++overflows; queue.clear(); frame->discontinuity = true;
                }
                queue.push_back(std::move(*frame));
                if (!scheduled) {
                    scheduled = true;
                    QTimer::singleShot(0, this, [this] { decodeNext(); });
                }
            }
        }
        if (!stopping && socket->hasPendingDatagrams() && !readScheduled) {
            readScheduled = true;
            QTimer::singleShot(0, this, [this] { readScheduled = false; read(); });
        }
    }

    void decodeNext()
    {
        scheduled = false;
        if (stopping || queue.empty()) return;
        auto input = std::move(queue.front()); queue.pop_front();
        QString error;
        auto output = decoder.decode(input, error);
        if (!error.isEmpty()) { ++errors; lastError = error; }
        for (auto &frame : output) {
            frame.sessionId = sessionId;
            frame.decodedMonotonicMs = monotonicMs();
            ++frames;
            QMutexLocker lock(&box->mutex);
            box->latest = std::move(frame);
            ++box->revision;
        }
        if (!queue.empty()) {
            scheduled = true;
            QTimer::singleShot(0, this, [this] { decodeNext(); });
        }
    }
};
} // 命名空间

struct VideoReceiver::Impl {
    std::shared_ptr<Mailbox> box = std::make_shared<Mailbox>();
    QThread thread;
    VideoWorker *worker = nullptr;
    QTimer timer;
    bool running = false;
    QString state = "disabled", sessionId, error;
    quint16 port = 0;
    quint64 seenRevision = 0;
};

VideoReceiver::VideoReceiver(QObject *parent) : QObject(parent), m(std::make_unique<Impl>())
{
    m->timer.setInterval(16);
    connect(&m->timer, &QTimer::timeout, this, &VideoReceiver::poll);
}
VideoReceiver::~VideoReceiver() { stop(); }

bool VideoReceiver::start(quint16 port, const QString &sessionId, QString &error,
                          const QHostAddress &bind, const QHostAddress &source)
{
    Q_ASSERT(QThread::currentThread() == thread());
    error.clear();
    if (m->running) return true;
    stop();
    m->box = std::make_shared<Mailbox>();
    m->sessionId = sessionId;
    m->port = port;
    m->seenRevision = 0;
    m->error.clear();
    if (bind.protocol() != QAbstractSocket::IPv4Protocol
        || (!source.isNull() && source.protocol() != QAbstractSocket::IPv4Protocol)) {
        error = "图传监听地址和来源地址必须为 IPv4";
        m->error = error; m->state = "error"; emit stateChanged(m->state); return false;
    }
    m->worker = new VideoWorker(m->box, sessionId);
    m->worker->moveToThread(&m->thread);
    const auto cleanup = connect(&m->thread, &QThread::finished, m->worker, &QObject::deleteLater);
    m->thread.start();
    bool ok = false;
    QMetaObject::invokeMethod(m->worker, [&] { ok = m->worker->start(port, bind, source, error); },
                              Qt::BlockingQueuedConnection);
    if (!ok) {
        m->thread.quit(); m->thread.wait(); m->worker = nullptr;
        disconnect(cleanup);
        m->state = "error"; m->error = error; emit stateChanged(m->state); return false;
    }
    m->running = true;
    m->state = "waiting";
    m->timer.start();
    emit stateChanged(m->state);
    return true;
}

void VideoReceiver::stop()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m->timer.stop();
    if (m->worker) {
        QMetaObject::invokeMethod(m->worker, [this] { m->worker->stop(); }, Qt::BlockingQueuedConnection);
        m->thread.quit(); m->thread.wait(); m->worker = nullptr;
    }
    const bool wasRunning = m->running;
    m->running = false;
    { QMutexLocker lock(&m->box->mutex); m->box->latest.reset(); }
    if (wasRunning) { m->state = "stopped"; emit stateChanged(m->state); }
}

void VideoReceiver::reset()
{
    stop();
    m->box = std::make_shared<Mailbox>();
    m->sessionId.clear();
    m->state = "disabled";
    m->error.clear();
    m->port = 0;
    m->seenRevision = 0;
}

std::optional<VideoFrame> VideoReceiver::latestFrame() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QMutexLocker lock(&m->box->mutex);
    if (!m->running || !m->box->latest || monotonicMs() - m->box->latest->receivedMonotonicMs > 500) return {};
    return m->box->latest;
}

QJsonObject VideoReceiver::snapshot() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QMutexLocker lock(&m->box->mutex);
    auto result = m->box->stats;
    QString state = m->state;
    if (m->running && m->box->latest) {
        const auto &frame = *m->box->latest;
        const auto age = monotonicMs() - frame.receivedMonotonicMs;
        state = age > 500 ? "stale" : "streaming";
        result.insert("frame_id", frame.frameId);
        result.insert("width", frame.image.width()); result.insert("height", frame.image.height());
        result.insert("frame_age_ms", age);
        result.insert("decode_latency_ms", frame.decodedMonotonicMs - frame.receivedMonotonicMs);
    }
    result.insert("state", state);
    result.insert("session_id", m->sessionId);
    if (!result.contains("port")) result.insert("port", m->port);
    if (!m->error.isEmpty()) result.insert("last_error", m->error);
    return result;
}

void VideoReceiver::poll()
{
    const auto state = snapshot().value("state").toString();
    if (state != m->state) { m->state = state; emit stateChanged(state); }
    quint64 revision;
    { QMutexLocker lock(&m->box->mutex); revision = m->box->revision; }
    if (revision != m->seenRevision) { m->seenRevision = revision; emit frameAvailable(); }
}
} // 命名空间 ly27
