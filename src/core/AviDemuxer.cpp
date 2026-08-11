#include "core/AviDemuxer.h"

#include "core/NalParser.h"

#include <QFile>
#include <cstring>

namespace h264tool {

DemuxResult AviDemuxer::load(const QString& path) {
    DemuxResult r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = QStringLiteral("无法打开 AVI: %1").arg(path);
        return r;
    }
    const QByteArray buf = f.readAll();
    if (buf.size() < 12 || std::memcmp(buf.constData(), "RIFF", 4) != 0) {
        r.error = QStringLiteral("不是有效的 RIFF/AVI 文件");
        return r;
    }

    // 查找 movi 列表
    int movi = buf.indexOf("movi");
    if (movi < 0) {
        r.error = QStringLiteral("AVI 中未找到 movi 列表");
        return r;
    }

    // 尝试从 strf/avih 猜测是否为 H264（宽松：直接扫块）
    const auto* data = reinterpret_cast<const uint8_t*>(buf.constData());
    int pos = movi + 4;
    const int end = buf.size();
    while (pos + 8 <= end) {
        const char* id = reinterpret_cast<const char*>(data + pos);
        const uint32_t sz = uint32_t(data[pos + 4]) | (uint32_t(data[pos + 5]) << 8) |
                            (uint32_t(data[pos + 6]) << 16) | (uint32_t(data[pos + 7]) << 24);
        pos += 8;
        if (pos + static_cast<int>(sz) > end) break;

        // xxdc / xxdb 通常为视频
        if (id[2] == 'd' && (id[3] == 'c' || id[3] == 'b') && sz > 4) {
            const uint8_t* chunk = data + pos;
            // 优先 Annex-B
            auto nals = NalParser::parseAnnexB(chunk, sz, static_cast<uint64_t>(pos));
            if (nals.empty()) {
                // 回退 AVCC 4 字节长度
                nals = NalParser::parseAvcc(chunk, sz, 4, static_cast<uint64_t>(pos));
            }
            r.nals.insert(r.nals.end(), nals.begin(), nals.end());
        }
        pos += static_cast<int>(sz);
        if (sz % 2) ++pos; // word align
    }

    if (r.nals.empty())
        r.error = QStringLiteral("AVI 中未提取到 H.264 NAL");
    r.fps = 25.0;
    return r;
}

} // namespace h264tool
