#pragma once

#include "core/RawH264Reader.h"

namespace h264tool {

/**
 * @brief 轻量 MP4 解复用：定位 vide 轨道、解析 avcC、从 sample 提取 AVCC NAL。
 *
 * 支持：
 * - progressive：stsz/stsc/stco(co64) 样本表
 * - fragmented（fMP4）：mvex/trex + moof/traf/tfhd/trun → mdat 样本
 */
class Mp4Demuxer {
public:
    static DemuxResult load(const QString& path);
};

} // namespace h264tool
