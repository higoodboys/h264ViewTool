#pragma once

#include "core/FrameInfo.h"
#include "core/NalUnit.h"

#include <QImage>
#include <QString>
#include <memory>
#include <vector>

namespace h264tool {

/**
 * @brief 解码器抽象：按帧索引输出 RGB 图像供预览窗口显示。
 */
class IDecoder {
public:
    virtual ~IDecoder() = default;

    virtual bool open(const QString& path) = 0;
    virtual void close() = 0;
    virtual int frameCount() const = 0;
    virtual QImage decodeFrame(int index) = 0;
    virtual QString backendName() const = 0;
    virtual QString lastError() const = 0;

    /**
     * @brief 注入打开文件时已解析的 NAL/帧索引（含 fileOffset 与关键帧标记）。
     * 解码器可据此从最近 I/IDR 起解，避免每次从文件头顺序重解。
     * @param nals 生命周期须覆盖本解码器使用期间（由 FileLoader 结果持有）
     */
    virtual void setStreamIndex(const std::vector<NalUnit>* nals,
                                const std::vector<FrameInfo>& frames) {
        (void)nals;
        (void)frames;
    }
};

} // namespace h264tool
