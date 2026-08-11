#pragma once

#include "core/RawH264Reader.h"

namespace h264tool {

/**
 * @brief 轻量 MP4 解复用：定位 vide 轨道、解析 avcC、从 mdat/sample 提取 AVCC NAL。
 *
 * 支持常见 avc1 progressive download（非 fragmented）。复杂 fMP4 可后续扩展或走 FFmpeg。
 */
class Mp4Demuxer {
public:
    static DemuxResult load(const QString& path);
};

} // namespace h264tool
