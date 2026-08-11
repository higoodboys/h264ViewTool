#pragma once

#include "core/NalUnit.h"

#include <cstdint>
#include <vector>

namespace h264tool {

/**
 * @brief NAL 分割：Annex-B 起始码 与 AVCC 长度前缀。
 */
class NalParser {
public:
    /**
     * 扫描 Annex-B：0x000001 或 0x00000001。
     * @param data 完整缓冲
     * @param baseOffset 该缓冲在文件中的起始偏移（用于填充 NalUnit::fileOffset）
     */
    static std::vector<NalUnit> parseAnnexB(const uint8_t* data, size_t size, uint64_t baseOffset = 0);

    /**
     * AVCC：每个 NAL 前有 lengthSize 字节大端长度。
     * @param lengthSize 通常为 4（来自 avcC.lengthSizeMinusOne+1）
     */
    static std::vector<NalUnit> parseAvcc(const uint8_t* data, size_t size,
                                          int lengthSize, uint64_t baseOffset = 0);

    /** 从 NAL 首字节填充类型字段 */
    static void fillHeaderFields(NalUnit& nal);
};

} // namespace h264tool
