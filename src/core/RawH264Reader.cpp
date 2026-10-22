#include "core/RawH264Reader.h"

#include "core/NalParser.h"

#include <QFile>

namespace h264tool {
namespace {

bool isPlausibleNalType(uint8_t nalHeader) {
    const uint8_t t = nalHeader & 0x1F;
    // 1..21 常见；23 保留；0/24+ 一般不作为裸流首 NAL
    return t >= 1 && t <= 21;
}

} // namespace

bool RawH264Reader::looksLikeAnnexB(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    // 允许前面有少量填充/自定义头，在前 4KB 内找起始码
    const QByteArray head = f.read(4096);
    if (head.size() < 4) return false;
    const auto* p = reinterpret_cast<const uint8_t*>(head.constData());
    const int n = head.size();
    for (int i = 0; i + 4 <= n; ++i) {
        int sc = 0;
        if (i + 4 <= n && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 0 && p[i + 3] == 1)
            sc = 4;
        else if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1)
            sc = 3;
        if (sc == 0) continue;
        if (i + sc >= n) break;
        if (isPlausibleNalType(p[i + sc]))
            return true;
    }
    return false;
}

DemuxResult RawH264Reader::load(const QString& path) {
    DemuxResult r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = QStringLiteral("无法打开文件: %1").arg(path);
        return r;
    }
    const QByteArray data = f.readAll();
    r.nals = NalParser::parseAnnexB(reinterpret_cast<const uint8_t*>(data.constData()),
                                    static_cast<size_t>(data.size()), 0);
    if (r.nals.empty())
        r.error = QStringLiteral("未找到 Annex-B NAL 起始码");
    return r;
}

} // namespace h264tool
