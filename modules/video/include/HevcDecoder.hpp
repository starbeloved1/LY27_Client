#pragma once

#include "VideoTypes.hpp"
#include <QList>
#include <memory>

namespace ly27 {

// 调用FFmpeg解码
class HevcDecoder {
public:
    HevcDecoder();
    ~HevcDecoder();
    HevcDecoder(const HevcDecoder &) = delete;
    HevcDecoder &operator=(const HevcDecoder &) = delete;
    bool start(QString &error);
    void reset();
    bool waitingForKeyframe() const;
    QList<VideoFrame> decode(const EncodedVideoFrame &frame, QString &error);
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // 命名空间 ly27
