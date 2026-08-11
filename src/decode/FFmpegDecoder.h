#pragma once

#include "decode/IDecoder.h"

#if H264TOOL_HAS_FFMPEG

#include <QString>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace h264tool {

/**
 * @brief 基于 FFmpeg 的软解。
 * 裸 H.264 不支持可靠 seek：回退到更早帧时完整重新打开文件再顺序解码。
 */
class FFmpegDecoder : public IDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    bool open(const QString& path) override;
    void close() override;
    int frameCount() const override { return frameCount_; }
    QImage decodeFrame(int index) override;
    QString backendName() const override { return QStringLiteral("FFmpeg"); }
    QString lastError() const override { return error_; }

private:
    bool rewindToStart();
    bool decodeNextFrame(int& decodedIndex);
    bool seekAndDecode(int index);
    QImage frameToImage(AVFrame* frame);
    void closeDemuxer();

    QString path_;
    bool rawStream_ = false;

    AVFormatContext* fmt_ = nullptr;
    AVCodecContext* codec_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;
    SwsContext* sws_ = nullptr;
    int videoStream_ = -1;
    int frameCount_ = 0;
    int lastIndex_ = -1;
    bool atStart_ = true;
    bool eofReached_ = false;
    QImage cache_;
    QString error_;
};

} // namespace h264tool
#endif
