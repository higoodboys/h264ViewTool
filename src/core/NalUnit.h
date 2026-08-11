#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace h264tool {

/** H.264 NAL unit type（常用子集） */
enum class NalType : uint8_t {
    Unspecified = 0,
    SliceNonIdr = 1,
    SliceDataA = 2,
    SliceDataB = 3,
    SliceDataC = 4,
    SliceIdr = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    Aud = 9,
    EndOfSeq = 10,
    EndOfStream = 11,
    Filler = 12,
    SpsExt = 13,
    Prefix = 14,
    SubSps = 15,
    Dps = 16,
    SliceAux = 19,
    SliceExt = 20,
    Unknown = 255
};

inline NalType nalTypeFromId(uint8_t id) {
    switch (id) {
    case 1: return NalType::SliceNonIdr;
    case 2: return NalType::SliceDataA;
    case 3: return NalType::SliceDataB;
    case 4: return NalType::SliceDataC;
    case 5: return NalType::SliceIdr;
    case 6: return NalType::Sei;
    case 7: return NalType::Sps;
    case 8: return NalType::Pps;
    case 9: return NalType::Aud;
    case 10: return NalType::EndOfSeq;
    case 11: return NalType::EndOfStream;
    case 12: return NalType::Filler;
    case 13: return NalType::SpsExt;
    case 14: return NalType::Prefix;
    case 15: return NalType::SubSps;
    case 16: return NalType::Dps;
    case 19: return NalType::SliceAux;
    case 20: return NalType::SliceExt;
    default: return NalType::Unknown;
    }
}

inline const char* nalTypeName(NalType t) {
    switch (t) {
    case NalType::SliceNonIdr: return "Slice(Non-IDR)";
    case NalType::SliceIdr: return "Slice(IDR)";
    case NalType::Sei: return "SEI";
    case NalType::Sps: return "SPS";
    case NalType::Pps: return "PPS";
    case NalType::Aud: return "AUD";
    default: return "Other";
    }
}

inline bool isVclNal(NalType t) {
    const auto id = static_cast<uint8_t>(t);
    return id >= 1 && id <= 5;
}

/**
 * @brief 单个 NAL 单元描述（载荷不含起始码/长度前缀）。
 */
struct NalUnit {
    NalType type = NalType::Unknown;
    uint8_t nalRefIdc = 0;   ///< 参考优先级（nal_ref_idc）
    uint8_t nalUnitTypeId = 0;
    uint64_t fileOffset = 0; ///< 在源文件中的字节偏移（含起始码或长度头起点）
    uint32_t headerSize = 0; ///< 起始码或长度字段字节数
    std::vector<uint8_t> payload; ///< NAL 头 + RBSP 原始字节（含 nal header）

    size_t totalSize() const { return headerSize + payload.size(); }

    /** 载荷中去掉 1 字节 NAL header 后的 EBSP */
    const uint8_t* ebspData() const {
        return payload.size() > 1 ? payload.data() + 1 : nullptr;
    }
    size_t ebspSize() const {
        return payload.size() > 1 ? payload.size() - 1 : 0;
    }
};

} // namespace h264tool
