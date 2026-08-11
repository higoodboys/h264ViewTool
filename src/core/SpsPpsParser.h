#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace h264tool {

struct SpsInfo {
    uint8_t profileIdc = 0;
    uint8_t levelIdc = 0;
    uint32_t spsId = 0;
    uint32_t chromaFormatIdc = 1;
    uint32_t log2MaxFrameNumMinus4 = 0;
    uint32_t picOrderCntType = 0;
    uint32_t log2MaxPicOrderCntLsbMinus4 = 0;
    bool deltaPicOrderAlwaysZeroFlag = false;
    int32_t offsetForNonRefPic = 0;
    int32_t offsetForTopToBottomField = 0;
    uint32_t numRefFramesInPicOrderCntCycle = 0;
    uint32_t maxNumRefFrames = 0;
    bool gapsInFrameNumValueAllowedFlag = false;
    uint32_t picWidthInMbsMinus1 = 0;
    uint32_t picHeightInMapUnitsMinus1 = 0;
    bool frameMbsOnlyFlag = true;
    bool mbAdaptiveFrameFieldFlag = false;
    bool direct8x8InferenceFlag = false;
    bool frameCroppingFlag = false;
    uint32_t frameCropLeft = 0, frameCropRight = 0, frameCropTop = 0, frameCropBottom = 0;
    bool vuiPresent = false;

    int width() const {
        int w = static_cast<int>(picWidthInMbsMinus1 + 1) * 16;
        if (frameCroppingFlag) {
            const int cropUnitX = (chromaFormatIdc == 1 || chromaFormatIdc == 2) ? 2 : 1;
            w -= static_cast<int>((frameCropLeft + frameCropRight) * cropUnitX);
        }
        return w;
    }

    int height() const {
        int h = static_cast<int>(picHeightInMapUnitsMinus1 + 1) * 16;
        if (!frameMbsOnlyFlag) h *= 2;
        if (frameCroppingFlag) {
            int cropUnitY = (chromaFormatIdc == 1) ? 2 : 1;
            if (!frameMbsOnlyFlag) cropUnitY *= 2;
            h -= static_cast<int>((frameCropTop + frameCropBottom) * cropUnitY);
        }
        return h;
    }

    std::string profileName() const;
};

struct PpsInfo {
    uint32_t ppsId = 0;
    uint32_t spsId = 0;
    bool entropyCodingModeFlag = false;
    bool bottomFieldPicOrderInFramePresentFlag = false;
    uint32_t numSliceGroupsMinus1 = 0;
    uint32_t numRefIdxL0DefaultActiveMinus1 = 0;
    uint32_t numRefIdxL1DefaultActiveMinus1 = 0;
    bool weightedPredFlag = false;
    uint8_t weightedBipredIdc = 0;
    int32_t picInitQpMinus26 = 0;
    int32_t picInitQsMinus26 = 0;
    int32_t chromaQpIndexOffset = 0;
    bool deblockingFilterControlPresentFlag = false;
    bool constrainedIntraPredFlag = false;
    bool redundantPicCntPresentFlag = false;
};

class SpsPpsParser {
public:
    static std::optional<SpsInfo> parseSps(const uint8_t* ebsp, size_t size);
    static std::optional<PpsInfo> parsePps(const uint8_t* ebsp, size_t size);
};

} // namespace h264tool
