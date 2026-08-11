#pragma once

#include "core/FrameInfo.h"
#include "core/NalUnit.h"
#include "core/StreamAnalyzer.h"

#include <QString>
#include <memory>
#include <vector>

namespace h264tool {

/**
 * @brief 统一文件加载入口：按扩展名/魔数选择 Raw / MP4 / AVI。
 */
class FileLoader {
public:
    struct LoadResult {
        QString path;
        std::vector<NalUnit> nals;
        StreamAnalyzer::Result analysis;
        QString error;
        bool ok() const { return error.isEmpty(); }
    };

    static LoadResult load(const QString& path);
};

} // namespace h264tool
