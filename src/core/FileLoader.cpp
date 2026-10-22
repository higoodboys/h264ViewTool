#include "core/FileLoader.h"

#include "core/AviDemuxer.h"
#include "core/Mp4Demuxer.h"
#include "core/RawH264Reader.h"

#include <QFile>
#include <QFileInfo>

namespace h264tool {
namespace {

bool looksLikeMp4(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray head = f.read(12);
    return head.size() >= 8 && head.mid(4, 4) == "ftyp";
}

bool looksLikeAvi(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray head = f.read(12);
    return head.startsWith("RIFF") && head.mid(8, 4) == "AVI ";
}

} // namespace

FileLoader::LoadResult FileLoader::load(const QString& path) {
    LoadResult out;
    out.path = path;
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        out.error = QStringLiteral("文件不存在: %1").arg(path);
        return out;
    }

    DemuxResult demux;

    // 一律按内容魔数选择，不依赖扩展名（裸流扩展名各异）
    if (looksLikeMp4(path)) {
        demux = Mp4Demuxer::load(path);
    } else if (looksLikeAvi(path)) {
        demux = AviDemuxer::load(path);
    } else if (RawH264Reader::looksLikeAnnexB(path)) {
        demux = RawH264Reader::load(path);
    } else {
        // 魔数不明：依次尝试
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
