#include "HevcDecoder.hpp"
#include "VideoAssembler.hpp"
#include <QMap>
#include <cstring>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

namespace ly27 {
namespace {
QString ffError(int result)
{
    char text[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(result, text, sizeof(text));
    return QString::fromUtf8(text);
}

bool hasIrap(const QByteArray &bytes)
{
    for (qsizetype i = 0; i + 4 < bytes.size(); ++i) {
        if (bytes[i] == 0 && bytes[i + 1] == 0 && bytes[i + 2] == 1) {
            const auto type = (static_cast<unsigned char>(bytes[i + 3]) >> 1) & 0x3f;
            if (type >= 16 && type <= 21) return true;
        }
    }
    return false;
}
} // 命名空间

struct HevcDecoder::Impl {
    AVCodecContext *codec = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    SwsContext *scale = nullptr;
    bool waiting = true;
    qint64 serial = 0;
    QMap<qint64, EncodedVideoFrame> sources;
    ~Impl()
    {
        sws_freeContext(scale);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec);
    }
};

HevcDecoder::HevcDecoder() : m(std::make_unique<Impl>()) {}
HevcDecoder::~HevcDecoder() = default;

bool HevcDecoder::start(QString &error)
{
    error.clear();
    m = std::make_unique<Impl>();
    const auto *decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!decoder) { error = "FFmpeg 未提供 HEVC 解码器"; return false; }
    m->codec = avcodec_alloc_context3(decoder);
    m->packet = av_packet_alloc();
    m->frame = av_frame_alloc();
    if (!m->codec || !m->packet || !m->frame) { error = "无法分配视频解码资源"; return false; }
    m->codec->thread_count = 2;
    m->codec->thread_type = FF_THREAD_SLICE;
    m->codec->flags |= AV_CODEC_FLAG_LOW_DELAY;
    // 在解码器内限制异常尺寸，避免先解码分配巨幅图像再检查。
    m->codec->max_pixels = 4096 * 2160;
    const int result = avcodec_open2(m->codec, decoder, nullptr);
    if (result < 0) { error = ffError(result); return false; }
    return true;
}

void HevcDecoder::reset()
{
    if (m->codec) avcodec_flush_buffers(m->codec);
    m->sources.clear();
    m->waiting = true;
}

bool HevcDecoder::waitingForKeyframe() const { return m->waiting; }

QList<VideoFrame> HevcDecoder::decode(const EncodedVideoFrame &input, QString &error)
{
    error.clear();
    QList<VideoFrame> result;
    if (!m->codec || !avcodec_is_open(m->codec)) { error = "解码器尚未启动"; return result; }
    if (input.discontinuity) reset();
    if (input.bytes.isEmpty() || input.bytes.size() > VideoAssembler::MaxFrameBytes) {
        error = "视频帧长度不合法"; reset(); return result;
    }
    if (m->waiting && !hasIrap(input.bytes)) return result;
    av_packet_unref(m->packet);
    int code = av_new_packet(m->packet, static_cast<int>(input.bytes.size()));
    if (code < 0) { error = ffError(code); reset(); return result; }
    memcpy(m->packet->data, input.bytes.constData(), static_cast<size_t>(input.bytes.size()));
    const auto serial = ++m->serial;
    m->packet->pts = serial;
    m->packet->dts = AV_NOPTS_VALUE;
    code = avcodec_send_packet(m->codec, m->packet);
    if (code < 0) { error = ffError(code); reset(); return result; }
    auto metadata = input;
    metadata.bytes.clear();
    m->sources.insert(serial, metadata);
    m->waiting = false;
    while ((code = avcodec_receive_frame(m->codec, m->frame)) >= 0) {
        const auto *frame = m->frame;
        if (frame->width < 1 || frame->height < 1 || frame->width > 4096 || frame->height > 2160
            || (frame->flags & AV_FRAME_FLAG_CORRUPT) || frame->decode_error_flags) {
            error = "解码图像损坏或尺寸超限"; break;
        }
        const auto source = m->sources.find(frame->pts);
        if (source == m->sources.end()) { error = "解码帧缺少时间来源"; break; }
        QImage image(frame->width, frame->height, QImage::Format_RGB888);
        if (image.isNull()) { error = "无法分配视频图像"; break; }
        m->scale = sws_getCachedContext(m->scale, frame->width, frame->height,
            static_cast<AVPixelFormat>(frame->format), frame->width, frame->height,
            AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!m->scale) { error = "无法创建像素转换器"; break; }
        uint8_t *planes[] = {image.bits(), nullptr, nullptr, nullptr};
        int strides[] = {static_cast<int>(image.bytesPerLine()), 0, 0, 0};
        if (sws_scale(m->scale, frame->data, frame->linesize, 0, frame->height, planes, strides) != frame->height) {
            error = "视频像素转换失败"; break;
        }
        VideoFrame output;
        output.image = std::move(image);
        output.frameId = source->frameId;
        output.receivedUtcMs = source->receivedUtcMs;
        output.receivedMonotonicMs = source->receivedMonotonicMs;
        result.append(std::move(output));
        m->sources.erase(source);
        av_frame_unref(m->frame);
    }
    if (error.isEmpty() && code != AVERROR(EAGAIN) && code != AVERROR_EOF) error = ffError(code);
    if (!error.isEmpty()) { result.clear(); reset(); }
    // 只保留有限的重排元数据，零输出的异常码流不能累积内存。
    if (m->sources.size() > 32) { error = "视频长期未输出可用帧"; reset(); }
    av_frame_unref(m->frame);
    return result;
}
} // 命名空间 ly27
