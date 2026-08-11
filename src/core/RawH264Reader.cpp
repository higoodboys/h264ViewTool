#include "core/RawH264Reader.h"

#include "core/NalParser.h"

#include <QFile>

namespace h264tool {

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
