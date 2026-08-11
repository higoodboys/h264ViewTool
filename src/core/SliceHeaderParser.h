#pragma once

#include "core/SpsPpsParser.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace h264tool {

enum class SliceTypeKind {
    P = 0,
    B = 1,
    I = 2,
    SP = 3,
    SI = 4,
    Unknown = 255
};

inline SliceTypeKind sliceTypeKind(uint32_t sliceType) {
    switch (sliceType % 5) {
    case 0: return SliceTypeKind::P;
    case 1: return SliceTypeKind::B;
    case 2: return SliceTypeKind::I;
    case 3: return SliceTypeKind::SP;
    case 4: return SliceTypeKind::SI;
    default: return SliceTypeKind::Unknown;
    }
}

inline const char* sliceTypeName(SliceTypeKind k) {
    switch (k) {
    case SliceTypeKind::P: return "P";
    case SliceTypeKind::B: return "B";
    case SliceTypeKind::I: return "I";
    case SliceTypeKind::SP: return "SP";
    case SliceTypeKind::SI: return "SI";
    default: return "?";
    }
}

struct SliceHeaderInfo {
    uint32_t firstMbInSlice = 0;
    uint32_t sliceType = 0;
    SliceTypeKind kind = SliceTypeKind::Unknown;
    uint32_t ppsId = 0;
    uint32_t frameNum = 0;
    bool fieldPicFlag = false;
    bool bottomFieldFlag = false;
    uint32_t idrPicId = 0;
    uint32_t picOrderCntLsb = 0;
    int32_t deltaPicOrderCntBottom = 0;
    int32_t deltaPicOrderCnt0 = 0;
    int32_t deltaPicOrderCnt1 = 0;
    bool numRefIdxActiveOverrideFlag = false;
    uint32_t numRefIdxL0ActiveMinus1 = 0;
    uint32_t numRefIdxL1ActiveMinus1 = 0;
    int32_t sliceQpDelta = 0;
    int qp = 26;
    bool isIdr = false;
    std::vector<int> refList0; ///< 简化：存放活跃 L0 数量占位索引 0..n
    std::vector<int> refList1;
};

class SliceHeaderParser {
public:
    /**
     * @param isIdr nal_unit_type==5
     * @param nalRefIdc 用于决定是否解析某些字段
     */
    static std::optional<SliceHeaderInfo> parse(const uint8_t* ebsp, size_t size,
                                                bool isIdr, uint8_t nalRefIdc,
                                                const SpsInfo& sps, const PpsInfo& pps);
};

} // namespace h264tool
