#include "core/StreamAnalyzer.h"

#include "core/SliceHeaderParser.h"

#include <algorithm>
#include <cmath>

namespace h264tool {

StreamAnalyzer::Result StreamAnalyzer::analyze(const std::vector<NalUnit>& nals, double fpsHint) {
    Result result;
    if (fpsHint <= 0.0) fpsHint = 25.0;

    SpsInfo currentSps;
    bool haveSps = false;
    PpsInfo currentPps;
    bool havePps = false;

    FrameInfo current;
    bool inFrame = false;
    int streamIndex = 0;

    auto flushFrame = [&]() {
        if (!inFrame) return;
        current.streamOrder = streamIndex++;
        current.displayOrder = current.poc; // 稍后重排
        result.frames.push_back(current);
        current = FrameInfo{};
        inFrame = false;
    };

    for (const auto& nal : nals) {
        if (nal.type == NalType::Sps) {
            if (auto sps = SpsPpsParser::parseSps(nal.ebspData(), nal.ebspSize())) {
                currentSps = *sps;
                haveSps = true;
                result.sps = currentSps;
                result.hasSps = true;
            }
            continue;
        }
        if (nal.type == NalType::Pps) {
            if (auto pps = SpsPpsParser::parsePps(nal.ebspData(), nal.ebspSize())) {
                result.ppsMap[pps->ppsId] = *pps;
                currentPps = *pps;
                havePps = true;
            }
            continue;
        }
        if (nal.type == NalType::Aud) {
            flushFrame();
            continue;
        }

        if (!isVclNal(nal.type)) continue;

        const bool isIdr = (nal.type == NalType::SliceIdr);
        SliceHeaderInfo sh;
        bool haveSh = false;
        if (haveSps && havePps) {
            // 使用 slice 中的 pps_id（需先读）；若失败仍用 currentPps
            if (auto parsed = SliceHeaderParser::parse(nal.ebspData(), nal.ebspSize(),
                                                       isIdr, nal.nalRefIdc,
                                                       currentSps, currentPps)) {
                sh = *parsed;
                haveSh = true;
                auto it = result.ppsMap.find(sh.ppsId);
                if (it != result.ppsMap.end() && it->second.ppsId != currentPps.ppsId) {
                    // 用正确 PPS 重解析一次以获得更准 QP
                    currentPps = it->second;
                    if (auto again = SliceHeaderParser::parse(nal.ebspData(), nal.ebspSize(),
                                                              isIdr, nal.nalRefIdc,
                                                              currentSps, currentPps)) {
                        sh = *again;
                    }
                }
            }
        }

        const bool newAu = !inFrame || (haveSh && sh.firstMbInSlice == 0);
        if (newAu) {
            flushFrame();
            inFrame = true;
            current.fileOffset = nal.fileOffset;
            current.isIdr = isIdr;
            if (haveSh) {
                current.type = frameTypeFromSlice(sh.kind);
                current.qp = sh.qp;
                current.poc = static_cast<int>(sh.picOrderCntLsb);
                current.frameNum = sh.frameNum;
                current.refList0 = sh.refList0;
                current.refList1 = sh.refList1;
            } else {
                current.type = isIdr ? FrameType::I : FrameType::Unknown;
            }
        }

        current.sizeBytes += nal.totalSize();
        if (isIdr) {
            current.isIdr = true;
            current.type = FrameType::I;
        }
    }
    flushFrame();

    // 按 POC 估算显示顺序
    std::vector<int> order(result.frames.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        if (result.frames[a].poc != result.frames[b].poc)
            return result.frames[a].poc < result.frames[b].poc;
        return a < b;
    });
    for (int display = 0; display < static_cast<int>(order.size()); ++display) {
        result.frames[order[display]].displayOrder = display;
        result.frames[order[display]].timeSec = display / fpsHint;
    }

    // 统计
    auto& st = result.stats;
    st.totalFrames = static_cast<int>(result.frames.size());
    st.fps = fpsHint;
    if (haveSps) {
        st.width = currentSps.width();
        st.height = currentSps.height();
        st.profile = QString::fromStdString(currentSps.profileName());
        st.level = QString::number(currentSps.levelIdc / 10.0, 'f', 1);
    }
    for (const auto& f : result.frames) {
        st.totalBytes += f.sizeBytes;
        switch (f.type) {
        case FrameType::I: ++st.iFrames; break;
        case FrameType::P: ++st.pFrames; break;
        case FrameType::B: ++st.bFrames; break;
        default: ++st.unknownFrames; break;
        }
    }
    if (st.totalFrames > 0)
        st.durationSec = st.totalFrames / fpsHint;
    if (st.durationSec > 0.0)
        st.avgBitrateKbps = (st.totalBytes * 8.0 / 1000.0) / st.durationSec;

    if (!haveSps)
        result.warning = QStringLiteral("未解析到 SPS，帧类型/分辨率可能不完整");

    return result;
}

} // namespace h264tool
