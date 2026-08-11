#pragma once

#include "core/SliceHeaderParser.h"

#include <QColor>
#include <QString>
#include <cstdint>
#include <vector>

namespace h264tool {

enum class FrameType {
    I,
    P,
    B,
    Unknown
};

inline const char* frameTypeName(FrameType t) {
    switch (t) {
    case FrameType::I: return "I";
    case FrameType::P: return "P";
    case FrameType::B: return "B";
    default: return "?";
    }
}

inline QColor frameTypeColor(FrameType t) {
    switch (t) {
    case FrameType::I: return QColor(0xE5, 0x39, 0x35); // 红
    case FrameType::P: return QColor(0x1E, 0x88, 0xE5); // 蓝
    case FrameType::B: return QColor(0x43, 0xA0, 0x47); // 绿
    default: return QColor(0x9E, 0x9E, 0x9E);
    }
}

inline FrameType frameTypeFromSlice(SliceTypeKind k) {
    switch (k) {
    case SliceTypeKind::I:
    case SliceTypeKind::SI: return FrameType::I;
    case SliceTypeKind::P:
    case SliceTypeKind::SP: return FrameType::P;
    case SliceTypeKind::B: return FrameType::B;
    default: return FrameType::Unknown;
    }
}

/**
 * @brief 一帧（Access Unit）的聚合信息，供条带图与信息面板使用。
 */
struct FrameInfo {
    int streamOrder = 0;   ///< 码流顺序（解码顺序）
    int displayOrder = 0;  ///< 显示顺序（由 POC 排序近似）
    int gap = 0;
    FrameType type = FrameType::Unknown;
    bool isIdr = false;
    uint64_t sizeBytes = 0;
    uint64_t fileOffset = 0;
    double timeSec = 0.0;
    int qp = -1;
    int poc = 0;
    uint32_t frameNum = 0;
    std::vector<int> refList0;
    std::vector<int> refList1;
    QString notes;

    QString typeLabel() const {
        if (isIdr) return QStringLiteral("IDR (I)");
        return QString::fromLatin1(frameTypeName(type));
    }
};

struct StreamStats {
    int totalFrames = 0;
    int iFrames = 0;
    int pFrames = 0;
    int bFrames = 0;
    int unknownFrames = 0;
    uint64_t totalBytes = 0;
    double durationSec = 0.0;
    double avgBitrateKbps = 0.0;
    int width = 0;
    int height = 0;
    QString profile;
    QString level;
    double fps = 0.0;
};

} // namespace h264tool
