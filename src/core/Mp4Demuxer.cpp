#include "core/Mp4Demuxer.h"

#include "core/NalParser.h"

#include <QFile>
#include <QByteArray>
#include <algorithm>
#include <cstring>
#include <vector>

namespace h264tool {
namespace {

uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
uint64_t be64(const uint8_t* p) {
    return (uint64_t(be32(p)) << 32) | be32(p + 4);
}

struct Atom {
    uint64_t offset = 0;
    uint64_t size = 0;
    char type[5] = {};
    uint64_t headerSize = 8;
};

bool readAtom(const QByteArray& buf, uint64_t& pos, Atom& a) {
    if (pos + 8 > static_cast<uint64_t>(buf.size())) return false;
    const auto* p = reinterpret_cast<const uint8_t*>(buf.constData()) + pos;
    uint64_t size = be32(p);
    std::memcpy(a.type, p + 4, 4);
    a.type[4] = 0;
    a.headerSize = 8;
    a.offset = pos;
    if (size == 1) {
        if (pos + 16 > static_cast<uint64_t>(buf.size())) return false;
        size = be64(p + 8);
        a.headerSize = 16;
    } else if (size == 0) {
        size = static_cast<uint64_t>(buf.size()) - pos;
    }
    if (size < a.headerSize) return false;
    a.size = size;
    pos += size;
    return true;
}

void findAtoms(const QByteArray& buf, uint64_t begin, uint64_t end,
               const char* type, std::vector<Atom>& out) {
    uint64_t pos = begin;
    while (pos + 8 <= end) {
        Atom a;
        uint64_t next = pos;
        if (!readAtom(buf, next, a)) break;
        if (std::strcmp(a.type, type) == 0) out.push_back(a);
        // 容器原子：递归常见类型
        if (std::strcmp(a.type, "moov") == 0 || std::strcmp(a.type, "trak") == 0 ||
            std::strcmp(a.type, "mdia") == 0 || std::strcmp(a.type, "minf") == 0 ||
            std::strcmp(a.type, "stbl") == 0 || std::strcmp(a.type, "edts") == 0) {
            findAtoms(buf, a.offset + a.headerSize, a.offset + a.size, type, out);
        }
        pos = a.offset + a.size;
        if (a.size == 0) break;
    }
}

QByteArray atomPayload(const QByteArray& buf, const Atom& a) {
    const int start = static_cast<int>(a.offset + a.headerSize);
    const int len = static_cast<int>(a.size - a.headerSize);
    if (start < 0 || len <= 0 || start + len > buf.size()) return {};
    return buf.mid(start, len);
}

struct AvcCConfig {
    int lengthSize = 4;
    QByteArray parameterSets; // SPS/PPS 以 AVCC 拼好，便于并入分析
};

bool parseAvcC(const QByteArray& avcC, AvcCConfig& cfg, std::vector<NalUnit>& outNals) {
    if (avcC.size() < 7) return false;
    const auto* p = reinterpret_cast<const uint8_t*>(avcC.constData());
    cfg.lengthSize = (p[4] & 0x3) + 1;
    int pos = 5;
    const int numSps = p[pos++] & 0x1F;
    for (int i = 0; i < numSps; ++i) {
        if (pos + 2 > avcC.size()) return false;
        const int len = (p[pos] << 8) | p[pos + 1];
        pos += 2;
        if (pos + len > avcC.size()) return false;
        NalUnit nal;
        nal.headerSize = 0;
        nal.fileOffset = 0;
        nal.payload.assign(p + pos, p + pos + len);
        NalParser::fillHeaderFields(nal);
        outNals.push_back(nal);
        pos += len;
    }
    if (pos >= avcC.size()) return true;
    const int numPps = p[pos++];
    for (int i = 0; i < numPps; ++i) {
        if (pos + 2 > avcC.size()) return false;
        const int len = (p[pos] << 8) | p[pos + 1];
        pos += 2;
        if (pos + len > avcC.size()) return false;
        NalUnit nal;
        nal.headerSize = 0;
        nal.fileOffset = 0;
        nal.payload.assign(p + pos, p + pos + len);
        NalParser::fillHeaderFields(nal);
        outNals.push_back(nal);
        pos += len;
    }
    return true;
}

} // namespace

DemuxResult Mp4Demuxer::load(const QString& path) {
    DemuxResult r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = QStringLiteral("无法打开 MP4: %1").arg(path);
        return r;
    }
    const QByteArray buf = f.readAll();
    if (buf.size() < 16) {
        r.error = QStringLiteral("文件过小，不是有效 MP4");
        return r;
    }

    std::vector<Atom> mdats, stsdAtoms, mdhdAtoms, stszAtoms, stcoAtoms, co64Atoms, stscAtoms;
    findAtoms(buf, 0, buf.size(), "mdat", mdats);
    findAtoms(buf, 0, buf.size(), "stsd", stsdAtoms);
    findAtoms(buf, 0, buf.size(), "mdhd", mdhdAtoms);
    findAtoms(buf, 0, buf.size(), "stsz", stszAtoms);
    findAtoms(buf, 0, buf.size(), "stco", stcoAtoms);
    findAtoms(buf, 0, buf.size(), "co64", co64Atoms);
    findAtoms(buf, 0, buf.size(), "stsc", stscAtoms);

    AvcCConfig avcCfg;
    bool foundAvc = false;
    for (const auto& stsd : stsdAtoms) {
        const QByteArray payload = atomPayload(buf, stsd);
        // 在 stsd 中搜索 avcC
        for (int i = 0; i + 8 < payload.size(); ++i) {
            if (std::memcmp(payload.constData() + i, "avcC", 4) == 0 && i >= 4) {
                const auto* sizePtr = reinterpret_cast<const uint8_t*>(payload.constData() + i - 4);
                const uint32_t boxSize = be32(sizePtr);
                if (boxSize >= 8 && i - 4 + static_cast<int>(boxSize) <= payload.size()) {
                    const QByteArray avcC = payload.mid(i + 4, static_cast<int>(boxSize) - 8);
                    if (parseAvcC(avcC, avcCfg, r.nals)) {
                        foundAvc = true;
                        break;
                    }
                }
            }
        }
        if (foundAvc) break;
    }
    if (!foundAvc) {
        r.error = QStringLiteral("未找到 H.264(avc1/avcC) 轨道");
        return r;
    }

    // 帧率：mdhd timescale + duration（粗略）
    if (!mdhdAtoms.empty()) {
        const QByteArray mdhd = atomPayload(buf, mdhdAtoms.front());
        if (mdhd.size() >= 20) {
            const auto* p = reinterpret_cast<const uint8_t*>(mdhd.constData());
            const uint8_t version = p[0];
            uint32_t timescale = 0;
            uint64_t duration = 0;
            if (version == 0 && mdhd.size() >= 20) {
                timescale = be32(p + 12);
                duration = be32(p + 16);
            } else if (version == 1 && mdhd.size() >= 32) {
                timescale = be32(p + 20);
                duration = be64(p + 24);
            }
            if (timescale > 0 && duration > 0) {
                // 样本数未知时先占位；后面用 sample 数修正
                r.fps = 0;
                (void)duration;
            }
            if (timescale > 0) {
                // 暂存 timescale 到 fps 字段占位，样本解析后再算
                r.fps = static_cast<double>(timescale);
            }
        }
    }

    // 解析 stsz / stco 顺序读取样本
    std::vector<uint32_t> sampleSizes;
    if (!stszAtoms.empty()) {
        const QByteArray stsz = atomPayload(buf, stszAtoms.front());
        if (stsz.size() >= 12) {
            const auto* p = reinterpret_cast<const uint8_t*>(stsz.constData());
            const uint32_t fixed = be32(p + 4);
            const uint32_t count = be32(p + 8);
            if (fixed != 0) {
                sampleSizes.assign(count, fixed);
            } else {
                if (stsz.size() < 12 + static_cast<int>(count) * 4) {
                    r.error = QStringLiteral("stsz 数据不完整");
                    return r;
                }
                sampleSizes.reserve(count);
                for (uint32_t i = 0; i < count; ++i)
                    sampleSizes.push_back(be32(p + 12 + i * 4));
            }
        }
    }

    std::vector<uint64_t> chunkOffsets;
    if (!stcoAtoms.empty()) {
        const QByteArray stco = atomPayload(buf, stcoAtoms.front());
        if (stco.size() >= 8) {
            const auto* p = reinterpret_cast<const uint8_t*>(stco.constData());
            const uint32_t count = be32(p + 4);
            for (uint32_t i = 0; i < count; ++i)
                chunkOffsets.push_back(be32(p + 8 + i * 4));
        }
    } else if (!co64Atoms.empty()) {
        const QByteArray co64 = atomPayload(buf, co64Atoms.front());
        if (co64.size() >= 8) {
            const auto* p = reinterpret_cast<const uint8_t*>(co64.constData());
            const uint32_t count = be32(p + 4);
            for (uint32_t i = 0; i < count; ++i)
                chunkOffsets.push_back(be64(p + 8 + i * 8));
        }
    }

    // stsc: sample-to-chunk
    struct StscEntry { uint32_t firstChunk, samplesPerChunk, desc; };
    std::vector<StscEntry> stsc;
    if (!stscAtoms.empty()) {
        const QByteArray body = atomPayload(buf, stscAtoms.front());
        if (body.size() >= 8) {
            const auto* p = reinterpret_cast<const uint8_t*>(body.constData());
            const uint32_t count = be32(p + 4);
            for (uint32_t i = 0; i < count; ++i) {
                StscEntry e;
                e.firstChunk = be32(p + 8 + i * 12);
                e.samplesPerChunk = be32(p + 12 + i * 12);
                e.desc = be32(p + 16 + i * 12);
                stsc.push_back(e);
            }
        }
    }

    if (sampleSizes.empty() || chunkOffsets.empty() || stsc.empty()) {
        // 回退：把整个 mdat 当 AVCC 连续流（部分文件可行）
        if (!mdats.empty()) {
            const QByteArray mdat = atomPayload(buf, mdats.front());
            auto more = NalParser::parseAvcc(reinterpret_cast<const uint8_t*>(mdat.constData()),
                                             static_cast<size_t>(mdat.size()),
                                             avcCfg.lengthSize,
                                             mdats.front().offset + mdats.front().headerSize);
            r.nals.insert(r.nals.end(), more.begin(), more.end());
        }
        if (r.fps > 1000) r.fps = 25.0; // timescale 误用修正
        return r;
    }

    auto samplesPerChunkFor = [&](uint32_t chunkIndex1Based) -> uint32_t {
        uint32_t spc = stsc.front().samplesPerChunk;
        for (size_t i = 0; i < stsc.size(); ++i) {
            if (chunkIndex1Based >= stsc[i].firstChunk)
                spc = stsc[i].samplesPerChunk;
            else
                break;
        }
        return spc;
    };

    size_t sampleIdx = 0;
    const auto* fileData = reinterpret_cast<const uint8_t*>(buf.constData());
    for (size_t ci = 0; ci < chunkOffsets.size(); ++ci) {
        const uint32_t spc = samplesPerChunkFor(static_cast<uint32_t>(ci + 1));
        uint64_t off = chunkOffsets[ci];
        for (uint32_t s = 0; s < spc && sampleIdx < sampleSizes.size(); ++s, ++sampleIdx) {
            const uint32_t sz = sampleSizes[sampleIdx];
            if (off + sz > static_cast<uint64_t>(buf.size())) break;
            auto nals = NalParser::parseAvcc(fileData + off, sz, avcCfg.lengthSize, off);
            r.nals.insert(r.nals.end(), nals.begin(), nals.end());
            off += sz;
        }
    }

    // fps：若先前存了 timescale，用样本数估算
    if (r.fps > 100.0 && !sampleSizes.empty()) {
        // 无法可靠得 duration 时默认 25
        r.fps = 25.0;
    } else if (r.fps <= 0.0) {
        r.fps = 25.0;
    }

    return r;
}

} // namespace h264tool
