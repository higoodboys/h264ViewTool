#include "core/FileLoader.h"

#include "core/AviDemuxer.h"
#include "core/Mp4Demuxer.h"
#include "core/RawH264Reader.h"

#include <QFile>
#include <QFileInfo>

namespace h264tool {

FileLoader::LoadResult FileLoader::load(const QString& path) {
    LoadResult out;
    out.path = path;
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        out.error = QStringLiteral("文件不存在: %1").arg(path);
        return out;
    }

    const QString suffix = fi.suffix().toLower();
    DemuxResult demux;

    auto looksLikeMp4 = [&]() {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return false;
        const QByteArray head = f.read(12);
        return head.size() >= 8 && head.mid(4, 4) == "ftyp";
    };
    auto looksLikeAvi = [&]() {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return false;
        const QByteArray head = f.read(12);
        return head.startsWith("RIFF") && head.mid(8, 4) == "AVI ";
    };

    if (suffix == QLatin1String("mp4") || suffix == QLatin1String("m4v") ||
        suffix == QLatin1String("mov") || looksLikeMp4()) {
        demux = Mp4Demuxer::load(path);
    } else if (suffix == QLatin1String("avi") || looksLikeAvi()) {
        demux = AviDemuxer::load(path);
    } else if (suffix == QLatin1String("h264") || suffix == QLatin1String("264") ||
               suffix == QLatin1String("annexb")) {
        demux = RawH264Reader::load(path);
    } else {
        // 未知扩展名：依次尝试
        demux = RawH264Reader::load(path);
        if (!demux.ok()) demux = Mp4Demuxer::load(path);
        if (!demux.ok()) demux = AviDemuxer::load(path);
    }

    if (!demux.ok()) {
        out.error = demux.error;
        return out;
    }

    out.nals = std::move(demux.nals);
    out.analysis = StreamAnalyzer::analyze(out.nals, demux.fps);
    if (!out.analysis.warning.isEmpty() && out.analysis.frames.empty()) {
        out.error = out.analysis.warning;
    }
    return out;
}

} // namespace h264tool
