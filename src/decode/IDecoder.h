#pragma once

#include <QImage>
#include <QString>
#include <memory>

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
};

} // namespace h264tool
