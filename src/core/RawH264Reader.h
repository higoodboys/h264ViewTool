#pragma once

#include "core/NalUnit.h"

#include <QString>
#include <vector>

namespace h264tool {

struct DemuxResult {
    std::vector<NalUnit> nals;
    double fps = 0.0;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

/** 裸 H.264 / Annex-B 文件读取 */
class RawH264Reader {
public:
    /** 按文件内容判断是否像 Annex-B（起始码 + 合法 NAL type），不依赖扩展名 */
    static bool looksLikeAnnexB(const QString& path);
    static DemuxResult load(const QString& path);
};

} // namespace h264tool
