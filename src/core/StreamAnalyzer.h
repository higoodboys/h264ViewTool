#pragma once

#include "core/FrameInfo.h"
#include "core/NalUnit.h"
#include "core/SpsPpsParser.h"

#include <QString>
#include <unordered_map>
#include <vector>

namespace h264tool {

/**
 * @brief 将 NAL 序列组装为帧列表，并计算全局统计。
 *
 * Access Unit 划分启发式：
 * - 遇到 AUD 开启新 AU；
 * - 或 VCL 且 first_mb_in_slice==0（新主键片）。
 */
class StreamAnalyzer {
public:
    struct Result {
        std::vector<FrameInfo> frames;
        StreamStats stats;
        SpsInfo sps;
        bool hasSps = false;
        std::unordered_map<uint32_t, PpsInfo> ppsMap;
        QString warning;
    };

    /**
     * @param nals 已分割的 NAL
     * @param fpsHint 若容器提供帧率则传入，用于时间轴；否则 0
     */
    static Result analyze(const std::vector<NalUnit>& nals, double fpsHint = 0.0);
};

} // namespace h264tool
