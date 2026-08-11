#pragma once

#include "decode/IDecoder.h"

namespace h264tool {

/** 无解码能力时的占位实现（未链接 FFmpeg 或打开失败） */
class NullDecoder : public IDecoder {
public:
    bool open(const QString& path) override;
    void close() override;
    int frameCount() const override { return frameCount_; }
    QImage decodeFrame(int index) override;
    QString backendName() const override { return QStringLiteral("Null"); }
    QString lastError() const override { return error_; }

    void setFrameCount(int n) { frameCount_ = n; }

private:
    int frameCount_ = 0;
    QString path_;
    QString error_;
};

} // namespace h264tool
