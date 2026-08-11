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
    static DemuxResult load(const QString& path);
};

} // namespace h264tool
