#include "decode/FFmpegDecoder.h"

#if H264TOOL_HAS_FFMPEG

#include <QFileInfo>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace h264tool {

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

    // 容器：先尝试时间基回绕
    if (!rawStream_ && fmt_ && codec_ && !eofReached_) {
        if (av_seek_frame(fmt_, videoStream_, 0, AVSEEK_FLAG_BACKWARD) >= 0) {
            avcodec_flush_buffers(codec_);
            lastIndex_ = -1;
            atStart_ = true;
            eofReached_ = false;
            return true;
        }
    }

    // 裸流 / seek 失败 / 已 EOF：必须重新打开（h264 demuxer 回绕后常一直 EOF）
    const QString path = path_;
    const int savedCount = frameCount_;
    const bool ok = open(path);
    if (ok && frameCount_ <= 0 && savedCount > 0)
        frameCount_ = savedCount;
    return ok;
}

bool FFmpegDecoder::open(const QString& path) {
    // 保留 sws_ 缓存；其余全部重建
    closeDemuxer();
    if (sws_) { sws_freeContext(sws_); sws_ = nullptr; }
    cache_ = QImage();
    path_ = path;
    error_.clear();

    const QByteArray pathUtf8 = path.toUtf8();
    const QString suffix = QFileInfo(path).suffix().toLower();

    rawStream_ = (suffix == QLatin1String("h264") || suffix == QLatin1String("264") ||
                  suffix == QLatin1String("annexb"));

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

    if (avcodec_open2(codec_, codec, nullptr) < 0) {
        error_ = QStringLiteral("avcodec_open2 失败");
        closeDemuxer();
        path_.clear();
        return false;
    }
    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    frameCount_ = static_cast<int>(st->nb_frames);
    if (frameCount_ <= 0 && st->duration > 0 && st->avg_frame_rate.num > 0) {
        const double dur = st->duration * av_q2d(st->time_base);
        const double fps = av_q2d(st->avg_frame_rate);
        frameCount_ = static_cast<int>(dur * fps + 0.5);
    }
    if (frameCount_ <= 0) frameCount_ = 0;
    lastIndex_ = -1;
    atStart_ = true;
    eofReached_ = false;
    error_.clear();
    return true;
}

void FFmpegDecoder::close() {
    closeDemuxer();
    if (sws_) { sws_freeContext(sws_); sws_ = nullptr; }
    frameCount_ = 0;
    cache_ = QImage();
    path_.clear();
    rawStream_ = false;
    error_.clear();
}

QImage FFmpegDecoder::frameToImage(AVFrame* frame) {
    const int w = frame->width;
    const int h = frame->height;
    if (w <= 0 || h <= 0) return QImage();

    sws_ = sws_getCachedContext(sws_, w, h, static_cast<AVPixelFormat>(frame->format),
                                w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR,
                                nullptr, nullptr, nullptr);
    if (!sws_) return QImage();

    QImage img(w, h, QImage::Format_RGB32);
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

bool FFmpegDecoder::seekAndDecode(int index) {
    if (!fmt_ || !codec_ || index < 0) return false;

    // 回退、跳跃过远、或已 EOF：需要回到文件头
    const bool needRewind =
        (lastIndex_ >= 0 && index <= lastIndex_) ||
        (lastIndex_ >= 0 && index > lastIndex_ + 32) ||
        eofReached_;

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
    if (!seekAndDecode(index)) {
        if (error_.isEmpty())
            error_ = QStringLiteral("解码帧 %1 失败").arg(index);
        return QImage();
    }
    error_.clear();
    return cache_;
}

} // namespace h264tool

#endif // H264TOOL_HAS_FFMPEG
