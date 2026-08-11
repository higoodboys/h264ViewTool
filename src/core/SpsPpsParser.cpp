#include "core/SpsPpsParser.h"

#include "core/BitReader.h"

namespace h264tool {

std::string SpsInfo::profileName() const {
    switch (profileIdc) {
    case 66: return "Baseline";
    case 77: return "Main";
    case 88: return "Extended";
    case 100: return "High";
    case 110: return "High10";
    case 122: return "High422";
    case 244: return "High444";
    default: return "Profile#" + std::to_string(profileIdc);
    }
}

namespace {

void skipScalingList(BitReader& br, int sizeOfList) {
    int32_t lastScale = 8;
    int32_t nextScale = 8;
    for (int i = 0; i < sizeOfList; ++i) {
        if (nextScale != 0) {
            const int32_t delta = br.readSe();
            nextScale = (lastScale + delta + 256) % 256;
        }
        lastScale = (nextScale == 0) ? lastScale : nextScale;
    }
}

} // namespace

std::optional<SpsInfo> SpsPpsParser::parseSps(const uint8_t* ebsp, size_t size) {
    if (!ebsp || size == 0) return std::nullopt;
    try {
        auto rbsp = ebspToRbsp(ebsp, size);
        BitReader br(rbsp);
        SpsInfo sps;
        sps.profileIdc = static_cast<uint8_t>(br.readBits(8));
        br.readBits(8); // constraint flags + reserved
        sps.levelIdc = static_cast<uint8_t>(br.readBits(8));
        sps.spsId = br.readUe();

        if (sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122 ||
            sps.profileIdc == 244 || sps.profileIdc == 44 || sps.profileIdc == 83 ||
            sps.profileIdc == 86 || sps.profileIdc == 118 || sps.profileIdc == 128 ||
            sps.profileIdc == 138 || sps.profileIdc == 139 || sps.profileIdc == 134) {
            sps.chromaFormatIdc = br.readUe();
            if (sps.chromaFormatIdc == 3) br.readFlag(); // separate_colour_plane_flag
            br.readUe(); // bit_depth_luma_minus8
            br.readUe(); // bit_depth_chroma_minus8
            br.readFlag(); // qpprime_y_zero_transform_bypass_flag
            if (br.readFlag()) { // seq_scaling_matrix_present_flag
                const int count = (sps.chromaFormatIdc != 3) ? 8 : 12;
                for (int i = 0; i < count; ++i) {
                    if (br.readFlag())
                        skipScalingList(br, i < 6 ? 16 : 64);
                }
            }
        }

        sps.log2MaxFrameNumMinus4 = br.readUe();
        sps.picOrderCntType = br.readUe();
        if (sps.picOrderCntType == 0) {
            sps.log2MaxPicOrderCntLsbMinus4 = br.readUe();
        } else if (sps.picOrderCntType == 1) {
            sps.deltaPicOrderAlwaysZeroFlag = br.readFlag();
            sps.offsetForNonRefPic = br.readSe();
            sps.offsetForTopToBottomField = br.readSe();
            sps.numRefFramesInPicOrderCntCycle = br.readUe();
            for (uint32_t i = 0; i < sps.numRefFramesInPicOrderCntCycle; ++i)
                br.readSe();
        }

        sps.maxNumRefFrames = br.readUe();
        sps.gapsInFrameNumValueAllowedFlag = br.readFlag();
        sps.picWidthInMbsMinus1 = br.readUe();
        sps.picHeightInMapUnitsMinus1 = br.readUe();
        sps.frameMbsOnlyFlag = br.readFlag();
        if (!sps.frameMbsOnlyFlag)
            sps.mbAdaptiveFrameFieldFlag = br.readFlag();
        sps.direct8x8InferenceFlag = br.readFlag();
        sps.frameCroppingFlag = br.readFlag();
        if (sps.frameCroppingFlag) {
            sps.frameCropLeft = br.readUe();
            sps.frameCropRight = br.readUe();
            sps.frameCropTop = br.readUe();
            sps.frameCropBottom = br.readUe();
        }
        sps.vuiPresent = br.readFlag();
        return sps;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<PpsInfo> SpsPpsParser::parsePps(const uint8_t* ebsp, size_t size) {
    if (!ebsp || size == 0) return std::nullopt;
    try {
        auto rbsp = ebspToRbsp(ebsp, size);
        BitReader br(rbsp);
        PpsInfo pps;
        pps.ppsId = br.readUe();
        pps.spsId = br.readUe();
        pps.entropyCodingModeFlag = br.readFlag();
        pps.bottomFieldPicOrderInFramePresentFlag = br.readFlag();
        pps.numSliceGroupsMinus1 = br.readUe();
        if (pps.numSliceGroupsMinus1 > 0) {
            // 简化：复杂 FMO 跳过完整解析，返回已读字段
            return pps;
        }
        pps.numRefIdxL0DefaultActiveMinus1 = br.readUe();
        pps.numRefIdxL1DefaultActiveMinus1 = br.readUe();
        pps.weightedPredFlag = br.readFlag();
        pps.weightedBipredIdc = static_cast<uint8_t>(br.readBits(2));
        pps.picInitQpMinus26 = br.readSe();
        pps.picInitQsMinus26 = br.readSe();
        pps.chromaQpIndexOffset = br.readSe();
        pps.deblockingFilterControlPresentFlag = br.readFlag();
        pps.constrainedIntraPredFlag = br.readFlag();
        pps.redundantPicCntPresentFlag = br.readFlag();
        return pps;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace h264tool
