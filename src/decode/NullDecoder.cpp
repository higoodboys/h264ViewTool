#include "decode/NullDecoder.h"

#include <QPainter>

namespace h264tool {

bool NullDecoder::open(const QString& path) {
    path_ = path;
    error_ = QStringLiteral("当前构建未启用可用解码后端，仅支持码流分析");
    return true; // 允许分析流程继续
}

void NullDecoder::close() {
    path_.clear();
}

QImage NullDecoder::decodeFrame(int index) {
    QImage img(640, 360, QImage::Format_RGB32);
    img.fill(QColor(32, 32, 36));
    QPainter p(&img);
    p.setPen(Qt::white);
    p.drawText(img.rect(), Qt::AlignCenter,
               QStringLiteral("无解码预览\n帧 #%1\n%2")
                   .arg(index)
                   .arg(error_));
    return img;
}

} // namespace h264tool
