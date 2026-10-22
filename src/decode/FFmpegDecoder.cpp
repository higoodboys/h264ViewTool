#include "decode/FFmpegDecoder.h"

#if H264TOOL_HAS_FFMPEG

#include "core/RawH264Reader.h"

#include <algorithm>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace h264tool {

namespace {
const char kStartCode4[] = {0x00, 0x00, 0x00, 0x01};
}

FFmpegDecoder::FFmpegDecoder() = default;

FFmpegDecoder::~FFmpegDecoder() {
    close();
}

void FFmpegDecoder::closeDemuxer() {
    if (packet_) { av_packet_free(&packet_); }
    if (frame_) { av_frame_free(&frame_); }
    if (codec_) { avcodec_free_context(&codec_); }
    if (fmt_) { avformat_close_input(&fmt_); }
    videoStream_ = -1;
    lastIndex_ = -1;
    atStart_ = true;
    eofReached_ = false;
}

bool FFmpegDecoder::rewindToStart() {
    if (path_.isEmpty()) return false;

    if (!rawStream_ && fmt_ && codec_ && !eofReached_) {
        if (av_seek_frame(fmt_, videoStream_, 0, AVSEEK_FLAG_BACKWARD) >= 0) {
            avcodec_flush_buffers(codec_);
            lastIndex_ = -1;
            atStart_ = true;
            eofReached_ = false;
            return true;
        }
    }

    const QString path = path_;
    const int savedCount = frameCount_;
    const bool ok = open(path);
    if (ok && frameCount_ <= 0 && savedCount > 0)
        frameCount_ = savedCount;
    return ok;
}

bool FFmpegDecoder::open(const QString& path) {
    closeDemuxer();
    if (sws_) { sws_freeContext(sws_); sws_ = nullptr; }
    cache_ = QImage();
    path_ = path;
    error_.clear();

    const QByteArray pathUtf8 = path.toUtf8();

    // 按内容识别裸流，不依赖扩展名
    rawStream_ = RawH264Reader::looksLikeAnnexB(path);

    const AVInputFormat* iformat = nullptr;
    if (rawStream_)
        iformat = av_find_input_format("h264");

    AVDictionary* opts = nullptr;
    if (rawStream_)
        av_dict_set(&opts, "framerate", "25", 0);

    if (avformat_open_input(&fmt_, pathUtf8.constData(), iformat, &opts) < 0) {
        av_dict_free(&opts);
        if (avformat_open_input(&fmt_, pathUtf8.constData(), nullptr, nullptr) < 0) {
            error_ = QStringLiteral("avformat_open_input 失败");
            path_.clear();
            return false;
        }
        rawStream_ = rawStream_ || (fmt_->iformat && QString::fromLatin1(fmt_->iformat->name) == QLatin1String("h264"));
    }
    av_dict_free(&opts);

    if (avformat_find_stream_info(fmt_, nullptr) < 0) {
        error_ = QStringLiteral("avformat_find_stream_info 失败");
        closeDemuxer();
        path_.clear();
        return false;
    }
    videoStream_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStream_ < 0) {
        error_ = QStringLiteral("未找到视频流");
        closeDemuxer();
        path_.clear();
        return false;
    }
    AVStream* st = fmt_->streams[videoStream_];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        error_ = QStringLiteral("找不到解码器");
        closeDemuxer();
        path_.clear();
        return false;
    }
    codec_ = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_, st->codecpar);
    codec_->err_recognition = 0;

    // avcC：configurationVersion==1 且 lengthSizeMinusOne 在 [4] 低 2 位
    if (codec_->extradata_size >= 5 && codec_->extradata && codec_->extradata[0] == 1)
        lengthSize_ = (codec_->extradata[4] & 0x3) + 1;
    else
        lengthSize_ = 4;

    detectPacketFormat();

    if (avcodec_open2(codec_, codec, nullptr) < 0) {
        error_ = QStringLiteral("avcodec_open2 失败");
        closeDemuxer();
        path_.clear();
        return false;
    }
    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    if (!aus_.empty())
        frameCount_ = static_cast<int>(aus_.size());
    else {
        frameCount_ = static_cast<int>(st->nb_frames);
        if (frameCount_ <= 0 && st->duration > 0 && st->avg_frame_rate.num > 0) {
            const double dur = st->duration * av_q2d(st->time_base);
            const double fps = av_q2d(st->avg_frame_rate);
            frameCount_ = static_cast<int>(dur * fps + 0.5);
        }
        if (frameCount_ <= 0) frameCount_ = 0;
    }
    lastIndex_ = -1;
    atStart_ = true;
    eofReached_ = false;
    error_.clear();
    return true;
}

void FFmpegDecoder::detectPacketFormat() {
    // 裸流 / 非 avcC extradata → Annex-B；MP4 的 avcC → AVCC
    const bool hasAvcc = codec_ && codec_->extradata_size >= 5 && codec_->extradata &&
                         codec_->extradata[0] == 1;
    useAnnexB_ = rawStream_ || !hasAvcc;
}

void FFmpegDecoder::close() {
    closeDemuxer();
    if (sws_) { sws_freeContext(sws_); sws_ = nullptr; }
    frameCount_ = 0;
    cache_ = QImage();
    path_.clear();
    rawStream_ = false;
    useAnnexB_ = false;
    error_.clear();
    nals_ = nullptr;
    aus_.clear();
}

void FFmpegDecoder::setStreamIndex(const std::vector<NalUnit>* nals,
                                   const std::vector<FrameInfo>& frames) {
    nals_ = nals;
    aus_.clear();
    aus_.reserve(frames.size());
    for (const auto& f : frames) {
        AuEntry e;
        e.nalBegin = f.nalBegin;
        e.nalEnd = f.nalEnd;
        e.fileOffset = f.fileOffset;
        e.isKeyframe = f.isIdr || f.type == FrameType::I;
        aus_.push_back(e);
    }
    if (!aus_.empty())
        frameCount_ = static_cast<int>(aus_.size());

    // 根据 NAL 头尺寸再校正：Annex-B 起始码为 3/4，AVCC 长度为 1..4 且文件为 MP4 时用 AVCC
    if (nals_ && !nals_->empty()) {
        const uint32_t hs = (*nals_)[0].headerSize;
        if (hs == 3 || hs == 4)
            useAnnexB_ = true;
        else if (codec_ && codec_->extradata_size >= 5 && codec_->extradata &&
                 codec_->extradata[0] == 1)
            useAnnexB_ = false;
    } else {
        detectPacketFormat();
    }

    lastIndex_ = -1;
    cache_ = QImage();
    if (codec_)
        avcodec_flush_buffers(codec_);
}

int FFmpegDecoder::findKeyframeAtOrBefore(int index) const {
    if (aus_.empty() || index < 0) return 0;
    if (index >= static_cast<int>(aus_.size()))
        index = static_cast<int>(aus_.size()) - 1;
    for (int i = index; i >= 0; --i) {
        if (aus_[static_cast<size_t>(i)].isKeyframe)
            return i;
    }
    return 0;
}

QByteArray FFmpegDecoder::buildAvccPacket(size_t nalBegin, size_t nalEnd) const {
    QByteArray pkt;
    if (!nals_ || nalBegin >= nalEnd || nalEnd > nals_->size())
        return pkt;
    const int ls = (lengthSize_ >= 1 && lengthSize_ <= 4) ? lengthSize_ : 4;
    for (size_t i = nalBegin; i < nalEnd; ++i) {
        const auto& nal = (*nals_)[i];
        const uint32_t len = static_cast<uint32_t>(nal.payload.size());
        if (len == 0) continue;
        char hdr[4];
        for (int b = 0; b < ls; ++b)
            hdr[b] = static_cast<char>((len >> (8 * (ls - 1 - b))) & 0xFF);
        pkt.append(hdr, ls);
        pkt.append(reinterpret_cast<const char*>(nal.payload.data()),
                   static_cast<int>(nal.payload.size()));
    }
    return pkt;
}

QByteArray FFmpegDecoder::buildAnnexBPacket(size_t nalBegin, size_t nalEnd) const {
    QByteArray pkt;
    if (!nals_ || nalBegin >= nalEnd || nalEnd > nals_->size())
        return pkt;
    for (size_t i = nalBegin; i < nalEnd; ++i) {
        const auto& nal = (*nals_)[i];
        if (nal.payload.empty()) continue;
        pkt.append(kStartCode4, 4);
        pkt.append(reinterpret_cast<const char*>(nal.payload.data()),
                   static_cast<int>(nal.payload.size()));
    }
    return pkt;
}

QByteArray FFmpegDecoder::paramSetsBefore(size_t nalIndex) const {
    QByteArray out;
    if (!nals_) return out;
    const size_t end = std::min(nalIndex, nals_->size());
    const std::vector<uint8_t>* sps = nullptr;
    const std::vector<uint8_t>* pps = nullptr;
    for (size_t i = 0; i < end; ++i) {
        const auto& nal = (*nals_)[i];
        if (nal.nalUnitTypeId == 7)
            sps = &nal.payload;
        else if (nal.nalUnitTypeId == 8)
            pps = &nal.payload;
    }
    // 若关键帧前没有，则在全文件中取最后一套
    if (!sps || !pps) {
        for (const auto& nal : *nals_) {
            if (nal.nalUnitTypeId == 7)
                sps = &nal.payload;
            else if (nal.nalUnitTypeId == 8)
                pps = &nal.payload;
        }
    }
    if (sps && !sps->empty()) {
        out.append(kStartCode4, 4);
        out.append(reinterpret_cast<const char*>(sps->data()), static_cast<int>(sps->size()));
    }
    if (pps && !pps->empty()) {
        out.append(kStartCode4, 4);
        out.append(reinterpret_cast<const char*>(pps->data()), static_cast<int>(pps->size()));
    }
    return out;
}

bool FFmpegDecoder::receiveOneFrame() {
    const int rr = avcodec_receive_frame(codec_, frame_);
    if (rr == 0) {
        atStart_ = false;
        return true;
    }
    return false;
}

bool FFmpegDecoder::sendPacketBytes(const QByteArray& pktData) {
    if (!codec_ || !packet_ || pktData.isEmpty()) return false;

    av_packet_unref(packet_);
    if (av_new_packet(packet_, pktData.size()) < 0) return false;
    std::memcpy(packet_->data, pktData.constData(), static_cast<size_t>(pktData.size()));

    const int sr = avcodec_send_packet(codec_, packet_);
    av_packet_unref(packet_);
    if (sr == 0) return true;
    if (sr != AVERROR(EAGAIN)) return false;

    while (receiveOneFrame()) {
    }
    if (av_new_packet(packet_, pktData.size()) < 0) return false;
    std::memcpy(packet_->data, pktData.constData(), static_cast<size_t>(pktData.size()));
    const int sr2 = avcodec_send_packet(codec_, packet_);
    av_packet_unref(packet_);
    return sr2 == 0;
}

bool FFmpegDecoder::decodeAuFromIndex(int auIndex, bool prependParamSets) {
    if (!codec_ || !packet_ || !nals_) return false;
    if (auIndex < 0 || auIndex >= static_cast<int>(aus_.size())) return false;
    const auto& au = aus_[static_cast<size_t>(auIndex)];

    QByteArray pktData;
    if (useAnnexB_) {
        if (prependParamSets)
            pktData = paramSetsBefore(au.nalBegin);
        pktData.append(buildAnnexBPacket(au.nalBegin, au.nalEnd));
    } else {
        pktData = buildAvccPacket(au.nalBegin, au.nalEnd);
    }
    if (pktData.isEmpty()) return false;

    packet_->pts = auIndex;
    packet_->dts = auIndex;
    if (!sendPacketBytes(pktData))
        return false;

    // 一包一帧：拿到首帧即停。继续 receive 若返回 EAGAIN 会把 frame 置空。
    const int rr = avcodec_receive_frame(codec_, frame_);
    if (rr == 0) {
        atStart_ = false;
        return frame_->width > 0 && frame_->height > 0;
    }
    return false;
}

bool FFmpegDecoder::seekAndDecodeIndexed(int index) {
    if (!codec_ || !nals_ || aus_.empty() || index < 0 ||
        index >= static_cast<int>(aus_.size())) {
        return false;
    }

    const int kf = findKeyframeAtOrBefore(index);
    int start = kf;
    bool restartFromKeyframe = true;

    if (lastIndex_ >= kf && lastIndex_ < index && !cache_.isNull()) {
        start = lastIndex_ + 1;
        restartFromKeyframe = false;
    } else {
        avcodec_flush_buffers(codec_);
        eofReached_ = false;
        start = kf;
        restartFromKeyframe = true;
    }

    for (int i = start; i <= index; ++i) {
        const bool prepend = useAnnexB_ && restartFromKeyframe && i == start;
        if (!decodeAuFromIndex(i, prepend)) {
            if (aus_[static_cast<size_t>(i)].isKeyframe || i == index) {
                error_ = QStringLiteral("索引解码帧 %1 失败 (from %2, keyframe %3, %4)")
                             .arg(index)
                             .arg(start)
                             .arg(kf)
                             .arg(useAnnexB_ ? QStringLiteral("AnnexB") : QStringLiteral("AVCC"));
                return false;
            }
            continue;
        }
    }

    cache_ = frameToImage(frame_);
    if (cache_.isNull()) {
        if (error_.isEmpty())
            error_ = QStringLiteral("帧 %1 色彩转换失败 (fmt=%2 %3x%4)")
                         .arg(index)
                         .arg(frame_ ? frame_->format : -1)
                         .arg(frame_ ? frame_->width : 0)
                         .arg(frame_ ? frame_->height : 0);
        return false;
    }
    lastIndex_ = index;
    return true;
}

QImage FFmpegDecoder::frameToImage(AVFrame* frame) {
    if (!frame) return QImage();
    const int w = frame->width;
    const int h = frame->height;
    if (w <= 0 || h <= 0) {
        error_ = QStringLiteral("解码帧尺寸无效 (%1x%2, fmt=%3)")
                     .arg(w)
                     .arg(h)
                     .arg(frame->format);
        return QImage();
    }

    AVPixelFormat srcFmt = static_cast<AVPixelFormat>(frame->format);
    if (srcFmt == AV_PIX_FMT_NONE)
        srcFmt = AV_PIX_FMT_YUV420P;

    sws_ = sws_getCachedContext(sws_, w, h, srcFmt, w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR,
                                nullptr, nullptr, nullptr);
    if (!sws_) return QImage();

    QImage img(w, h, QImage::Format_RGB32);
    if (img.isNull() || !img.bits()) return QImage();
    uint8_t* dst[] = { img.bits() };
    int dstStride[] = { static_cast<int>(img.bytesPerLine()) };
    sws_scale(sws_, frame->data, frame->linesize, 0, h, dst, dstStride);
    return img;
}

bool FFmpegDecoder::decodeNextFrame(int& decodedIndex) {
    for (;;) {
        const int ret = av_read_frame(fmt_, packet_);
        if (ret < 0) {
            eofReached_ = true;
            avcodec_send_packet(codec_, nullptr);
            while (avcodec_receive_frame(codec_, frame_) == 0) {
                ++decodedIndex;
                return true;
            }
            return false;
        }
        if (packet_->stream_index != videoStream_) {
            av_packet_unref(packet_);
            continue;
        }
        if (avcodec_send_packet(codec_, packet_) < 0) {
            av_packet_unref(packet_);
            continue;
        }
        av_packet_unref(packet_);

        if (avcodec_receive_frame(codec_, frame_) == 0) {
            ++decodedIndex;
            atStart_ = false;
            return true;
        }
    }
}

bool FFmpegDecoder::seekAndDecodeDemux(int index) {
    if (!fmt_ || !codec_ || index < 0) return false;

    const bool needRewind =
        (lastIndex_ >= 0 && index <= lastIndex_) || eofReached_;

    if (needRewind) {
        if (!rewindToStart()) {
            error_ = QStringLiteral("回绕到文件头失败，无法解码帧 %1").arg(index);
            return false;
        }
    }

    int decodedIndex = lastIndex_;
    while (decodedIndex < index) {
        if (!decodeNextFrame(decodedIndex)) {
            error_ = QStringLiteral("解码到帧 %1 时遇到 EOF/错误 (已得 %2)")
                         .arg(index)
                         .arg(decodedIndex);
            return false;
        }
        if (decodedIndex == index) {
            cache_ = frameToImage(frame_);
            if (cache_.isNull()) {
                error_ = QStringLiteral("帧 %1 色彩转换失败").arg(index);
                return false;
            }
            lastIndex_ = index;
            return true;
        }
    }
    return lastIndex_ == index && !cache_.isNull();
}

QImage FFmpegDecoder::decodeFrame(int index) {
    if (index == lastIndex_ && !cache_.isNull()) return cache_;

    bool ok = false;
    if (nals_ && !aus_.empty())
        ok = seekAndDecodeIndexed(index);
    else
        ok = seekAndDecodeDemux(index);

    if (!ok) {
        if (error_.isEmpty())
            error_ = QStringLiteral("解码帧 %1 失败").arg(index);
        return QImage();
    }
    error_.clear();
    return cache_;
}

} // namespace h264tool

#endif // H264TOOL_HAS_FFMPEG
