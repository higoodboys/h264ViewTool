#pragma once

#include "decode/IDecoder.h"

#if H264TOOL_HAS_FFMPEG

#include <QByteArray>
#include <QString>
#include <vector>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace h264tool {

/**
 * @brief 基于 FFmpeg 的软解。
 *
 * 若调用 setStreamIndex，则按已解析的 NAL/关键帧索引在内存中喂给解码器：
 * - MP4/AVCC：长度前缀包
 * - 裸流 Annex-B：起始码包，关键帧前补 SPS/PPS
 * 跳转时从最近 I/IDR 解到目标帧，无需从文件头重解。
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
    void setStreamIndex(const std::vector<NalUnit>* nals,
                        const std::vector<FrameInfo>& frames) override;

private:
    struct AuEntry {
        size_t nalBegin = 0;
        size_t nalEnd = 0;
        uint64_t fileOffset = 0;
        bool isKeyframe = false;
    };

    bool rewindToStart();
    bool decodeNextFrame(int& decodedIndex);
    bool seekAndDecodeDemux(int index);
    bool seekAndDecodeIndexed(int index);
    bool decodeAuFromIndex(int auIndex, bool prependParamSets);
    bool sendPacketBytes(const QByteArray& pktData);
    bool receiveOneFrame();
    QByteArray buildAvccPacket(size_t nalBegin, size_t nalEnd) const;
    QByteArray buildAnnexBPacket(size_t nalBegin, size_t nalEnd) const;
    QByteArray paramSetsBefore(size_t nalIndex) const;
    void detectPacketFormat();
    int findKeyframeAtOrBefore(int index) const;
    QImage frameToImage(AVFrame* frame);
    void closeDemuxer();

    QString path_;
    bool rawStream_ = false;
    bool useAnnexB_ = false; ///< 索引喂包用 Annex-B（裸流）；否则 AVCC

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

    const std::vector<NalUnit>* nals_ = nullptr;
    std::vector<AuEntry> aus_;
    int lengthSize_ = 4;
};

} // namespace h264tool
#endif
