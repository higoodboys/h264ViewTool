#include "core/SliceHeaderParser.h"

#include "core/BitReader.h"

namespace h264tool {

std::optional<SliceHeaderInfo> SliceHeaderParser::parse(const uint8_t* ebsp, size_t size,
                                                        bool isIdr, uint8_t nalRefIdc,
                                                        const SpsInfo& sps, const PpsInfo& pps) {
    if (!ebsp || size == 0) return std::nullopt;
    try {
        auto rbsp = ebspToRbsp(ebsp, size);
        BitReader br(rbsp);
        SliceHeaderInfo sh;
        sh.isIdr = isIdr;
        sh.firstMbInSlice = br.readUe();
        sh.sliceType = br.readUe();
        sh.kind = sliceTypeKind(sh.sliceType);
        if (isIdr) sh.kind = SliceTypeKind::I;
        sh.ppsId = br.readUe();

        const int frameNumBits = static_cast<int>(sps.log2MaxFrameNumMinus4 + 4);
        sh.frameNum = br.readBits(frameNumBits);

        if (!sps.frameMbsOnlyFlag) {
            sh.fieldPicFlag = br.readFlag();
            if (sh.fieldPicFlag) sh.bottomFieldFlag = br.readFlag();
        }

        if (isIdr) sh.idrPicId = br.readUe();

        if (sps.picOrderCntType == 0) {
            const int pocBits = static_cast<int>(sps.log2MaxPicOrderCntLsbMinus4 + 4);
            sh.picOrderCntLsb = br.readBits(pocBits);
            if (pps.bottomFieldPicOrderInFramePresentFlag && !sh.fieldPicFlag)
                sh.deltaPicOrderCntBottom = br.readSe();
        } else if (sps.picOrderCntType == 1 && !sps.deltaPicOrderAlwaysZeroFlag) {
            sh.deltaPicOrderCnt0 = br.readSe();
            if (pps.bottomFieldPicOrderInFramePresentFlag && !sh.fieldPicFlag)
                sh.deltaPicOrderCnt1 = br.readSe();
        }

        if (pps.redundantPicCntPresentFlag) br.readUe();

        if (sh.kind == SliceTypeKind::B) br.readFlag(); // direct_spatial_mv_pred_flag

        sh.numRefIdxL0ActiveMinus1 = pps.numRefIdxL0DefaultActiveMinus1;
        sh.numRefIdxL1ActiveMinus1 = pps.numRefIdxL1DefaultActiveMinus1;

        if (sh.kind == SliceTypeKind::P || sh.kind == SliceTypeKind::SP || sh.kind == SliceTypeKind::B) {
            sh.numRefIdxActiveOverrideFlag = br.readFlag();
            if (sh.numRefIdxActiveOverrideFlag) {
                sh.numRefIdxL0ActiveMinus1 = br.readUe();
                if (sh.kind == SliceTypeKind::B)
                    sh.numRefIdxL1ActiveMinus1 = br.readUe();
            }
        }

        // ref_pic_list_modification() —— 简化跳过详细修改语法，仅记录活跃数量
        auto skipRefPicListModification = [&](bool isL1) {
            (void)isL1;
            if (!br.readFlag()) return; // ref_pic_list_modification_flag
            for (;;) {
                const uint32_t idc = br.readUe();
                if (idc == 3) break;
                if (idc == 0 || idc == 1) br.readUe();
                else if (idc == 2) br.readUe();
                else break;
            }
        };

        if (sh.kind != SliceTypeKind::I && sh.kind != SliceTypeKind::SI)
            skipRefPicListModification(false);
        if (sh.kind == SliceTypeKind::B)
            skipRefPicListModification(true);

        // 加权预测表：简化跳过
        if ((pps.weightedPredFlag && (sh.kind == SliceTypeKind::P || sh.kind == SliceTypeKind::SP)) ||
            (pps.weightedBipredIdc == 1 && sh.kind == SliceTypeKind::B)) {
            // pred_weight_table 较复杂，尽力跳过会导致不同步；此处中止精细字段，QP 用默认
            sh.qp = 26 + pps.picInitQpMinus26;
            for (uint32_t i = 0; i <= sh.numRefIdxL0ActiveMinus1; ++i)
                sh.refList0.push_back(static_cast<int>(i));
            if (sh.kind == SliceTypeKind::B) {
                for (uint32_t i = 0; i <= sh.numRefIdxL1ActiveMinus1; ++i)
                    sh.refList1.push_back(static_cast<int>(i));
            }
            return sh;
        }

        if (nalRefIdc != 0) {
            if (isIdr) {
                br.readFlag(); // no_output_of_prior_pics_flag
                br.readFlag(); // long_term_reference_flag
            } else {
                if (br.readFlag()) { // adaptive_ref_pic_marking_mode_flag
                    for (;;) {
                        const uint32_t op = br.readUe();
                        if (op == 0) break;
                        if (op == 1 || op == 3) br.readUe();
                        if (op == 2) br.readUe();
                        if (op == 3 || op == 6) br.readUe();
                        if (op == 4) br.readUe();
                        if (op == 5) { /* none */ }
                    }
                }
            }
        }

        if (pps.entropyCodingModeFlag && sh.kind != SliceTypeKind::I && sh.kind != SliceTypeKind::SI)
            br.readUe(); // cabac_init_idc

        sh.sliceQpDelta = br.readSe();
        sh.qp = 26 + pps.picInitQpMinus26 + sh.sliceQpDelta;

        for (uint32_t i = 0; i <= sh.numRefIdxL0ActiveMinus1 && sh.kind != SliceTypeKind::I; ++i)
            sh.refList0.push_back(static_cast<int>(i));
        if (sh.kind == SliceTypeKind::B) {
            for (uint32_t i = 0; i <= sh.numRefIdxL1ActiveMinus1; ++i)
                sh.refList1.push_back(static_cast<int>(i));
        }
        return sh;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace h264tool
