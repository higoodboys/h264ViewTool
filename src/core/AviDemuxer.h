#pragma once

#include "core/RawH264Reader.h"

namespace h264tool {

/**
 * @brief 简易 AVI 读取：在 movi 列表中查找 0xdc / 压缩为 H264/AVC1 的块。
 */
class AviDemuxer {
public:
    static DemuxResult load(const QString& path);
};

} // namespace h264tool
