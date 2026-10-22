#include "core/Mp4Demuxer.h"

#include "core/NalParser.h"

#include <QFile>
#include <QByteArray>
#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>

namespace h264tool {
namespace {

uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
uint64_t be64(const uint8_t* p) {
    return (uint64_t(be32(p)) << 32) | be32(p + 4);
}
int32_t be32s(const uint8_t* p) {
    return static_cast<int32_t>(be32(p));
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
        if (std::strcmp(a.type, "moov") == 0 || std::strcmp(a.type, "trak") == 0 ||
            std::strcmp(a.type, "mdia") == 0 || std::strcmp(a.type, "minf") == 0 ||
            std::strcmp(a.type, "stbl") == 0 || std::strcmp(a.type, "edts") == 0 ||
            std::strcmp(a.type, "mvex") == 0 || std::strcmp(a.type, "moof") == 0 ||
            std::strcmp(a.type, "traf") == 0) {
            findAtoms(buf, a.offset + a.headerSize, a.offset + a.size, type, out);
        }
        pos = a.offset + a.size;
        if (a.size == 0) break;
    }
}

QByteArray atomPayload(const QByteArray& buf, const Atom& a) {
    const qint64 start = static_cast<qint64>(a.offset + a.headerSize);
    const qint64 len = static_cast<qint64>(a.size - a.headerSize);
    if (start < 0 || len <= 0 || start + len > buf.size()) return {};
    return buf.mid(static_cast<int>(start), static_cast<int>(len));
}

const uint8_t* atomPayloadPtr(const QByteArray& buf, const Atom& a, uint64_t& outLen) {
    const uint64_t start = a.offset + a.headerSize;
    const uint64_t len = a.size - a.headerSize;
    if (start + len > static_cast<uint64_t>(buf.size())) {
        outLen = 0;
        return nullptr;
    }
    outLen = len;
    return reinterpret_cast<const uint8_t*>(buf.constData()) + start;
}

struct AvcCConfig {
    int lengthSize = 4;
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

struct TrexDefaults {
    uint32_t defaultSampleDescriptionIndex = 1;
    uint32_t defaultSampleDuration = 0;
    uint32_t defaultSampleSize = 0;
    uint32_t defaultSampleFlags = 0;
};

struct TrackInfo {
    uint32_t trackId = 0;
    Atom trak;
    bool isVideo = false;
    uint32_t timescale = 0;
    uint64_t duration = 0;
};

std::optional<uint32_t> parseTkhdTrackId(const QByteArray& buf, const Atom& tkhd) {
    uint64_t len = 0;
    const uint8_t* p = atomPayloadPtr(buf, tkhd, len);
    if (!p || len < 16) return std::nullopt;
    const uint8_t version = p[0];
    if (version == 0 && len >= 16)
        return be32(p + 12);
    if (version == 1 && len >= 24)
        return be32(p + 20);
    return std::nullopt;
}

bool parseMdhd(const QByteArray& buf, const Atom& mdhd, uint32_t& timescale, uint64_t& duration) {
    uint64_t len = 0;
    const uint8_t* p = atomPayloadPtr(buf, mdhd, len);
    if (!p || len < 20) return false;
    const uint8_t version = p[0];
    if (version == 0 && len >= 20) {
        timescale = be32(p + 12);
        duration = be32(p + 16);
        return true;
    }
    if (version == 1 && len >= 32) {
        timescale = be32(p + 20);
        duration = be64(p + 24);
        return true;
    }
    return false;
}

bool hdlrIsVide(const QByteArray& buf, const Atom& hdlr) {
    uint64_t len = 0;
    const uint8_t* p = atomPayloadPtr(buf, hdlr, len);
    // version/flags(4) + pre_defined(4) + handler_type(4)
    if (!p || len < 12) return false;
    return std::memcmp(p + 8, "vide", 4) == 0;
}

std::vector<TrackInfo> collectTracks(const QByteArray& buf) {
    std::vector<TrackInfo> tracks;
    std::vector<Atom> traks;
    findAtoms(buf, 0, static_cast<uint64_t>(buf.size()), "trak", traks);
    for (const auto& trak : traks) {
        TrackInfo ti;
        ti.trak = trak;
        std::vector<Atom> tkhds, hdlrs, mdhds;
        findAtoms(buf, trak.offset + trak.headerSize, trak.offset + trak.size, "tkhd", tkhds);
        findAtoms(buf, trak.offset + trak.headerSize, trak.offset + trak.size, "hdlr", hdlrs);
        findAtoms(buf, trak.offset + trak.headerSize, trak.offset + trak.size, "mdhd", mdhds);
        if (tkhds.empty()) continue;
        const auto tid = parseTkhdTrackId(buf, tkhds.front());
        if (!tid) continue;
        ti.trackId = *tid;
        ti.isVideo = !hdlrs.empty() && hdlrIsVide(buf, hdlrs.front());
        if (!mdhds.empty())
            parseMdhd(buf, mdhds.front(), ti.timescale, ti.duration);
        tracks.push_back(ti);
    }
    return tracks;
}

struct StscEntry {
    uint32_t firstChunk = 0;
    uint32_t samplesPerChunk = 0;
    uint32_t desc = 0;
};

bool parseStsz(const QByteArray& buf, const Atom& stsz, std::vector<uint32_t>& sampleSizes) {
    uint64_t len = 0;
    const uint8_t* p = atomPayloadPtr(buf, stsz, len);
    if (!p || len < 12) return false;
    const uint32_t fixed = be32(p + 4);
    const uint32_t count = be32(p + 8);
    if (fixed != 0) {
        sampleSizes.assign(count, fixed);
        return true;
    }
    if (len < 12ull + uint64_t(count) * 4ull) return false;
    sampleSizes.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        sampleSizes.push_back(be32(p + 12 + i * 4));
    return true;
}

bool parseChunkOffsets(const QByteArray& buf, const Atom* stco, const Atom* co64,
                       std::vector<uint64_t>& chunkOffsets) {
    if (stco) {
        uint64_t len = 0;
        const uint8_t* p = atomPayloadPtr(buf, *stco, len);
        if (!p || len < 8) return false;
        const uint32_t count = be32(p + 4);
        if (len < 8ull + uint64_t(count) * 4ull) return false;
        for (uint32_t i = 0; i < count; ++i)
            chunkOffsets.push_back(be32(p + 8 + i * 4));
        return true;
    }
    if (co64) {
        uint64_t len = 0;
        const uint8_t* p = atomPayloadPtr(buf, *co64, len);
        if (!p || len < 8) return false;
        const uint32_t count = be32(p + 4);
        if (len < 8ull + uint64_t(count) * 8ull) return false;
        for (uint32_t i = 0; i < count; ++i)
            chunkOffsets.push_back(be64(p + 8 + i * 8));
        return true;
    }
    return false;
}

bool parseStsc(const QByteArray& buf, const Atom& stsc, std::vector<StscEntry>& out) {
    uint64_t len = 0;
    const uint8_t* p = atomPayloadPtr(buf, stsc, len);
    if (!p || len < 8) return false;
    const uint32_t count = be32(p + 4);
    if (len < 8ull + uint64_t(count) * 12ull) return false;
    for (uint32_t i = 0; i < count; ++i) {
        StscEntry e;
        e.firstChunk = be32(p + 8 + i * 12);
        e.samplesPerChunk = be32(p + 12 + i * 12);
        e.desc = be32(p + 16 + i * 12);
        out.push_back(e);
    }
    return !out.empty();
}

void parseTrexMap(const QByteArray& buf, std::vector<std::pair<uint32_t, TrexDefaults>>& trex) {
    std::vector<Atom> atoms;
    findAtoms(buf, 0, static_cast<uint64_t>(buf.size()), "trex", atoms);
    for (const auto& a : atoms) {
        uint64_t len = 0;
        const uint8_t* p = atomPayloadPtr(buf, a, len);
        if (!p || len < 24) continue;
        TrexDefaults d;
        const uint32_t trackId = be32(p + 4);
        d.defaultSampleDescriptionIndex = be32(p + 8);
        d.defaultSampleDuration = be32(p + 12);
        d.defaultSampleSize = be32(p + 16);
        d.defaultSampleFlags = be32(p + 20);
        trex.emplace_back(trackId, d);
    }
}

const TrexDefaults* findTrex(const std::vector<std::pair<uint32_t, TrexDefaults>>& trex,
                             uint32_t trackId) {
    for (const auto& e : trex) {
        if (e.first == trackId) return &e.second;
    }
    return nullptr;
}

struct FragExtractResult {
    std::vector<std::pair<uint64_t, uint32_t>> samples;
    uint64_t durationTicks = 0; // 本 moof 视频轨样本时长合计（若可知）
    bool hasDuration = false;
    uint64_t tfdt = 0;
    bool hasTfdt = false;
};

/** 从 moof 内 traf/trun 提取指定视频轨样本的文件偏移与大小 */
bool extractFragmentSamples(const QByteArray& buf, const Atom& moof, uint32_t videoTrackId,
                            const TrexDefaults* trex, FragExtractResult& out) {
    std::vector<Atom> trafs;
    findAtoms(buf, moof.offset + moof.headerSize, moof.offset + moof.size, "traf", trafs);

    for (const auto& traf : trafs) {
        std::vector<Atom> tfhds, truns, tfdts;
        findAtoms(buf, traf.offset + traf.headerSize, traf.offset + traf.size, "tfhd", tfhds);
        findAtoms(buf, traf.offset + traf.headerSize, traf.offset + traf.size, "trun", truns);
        findAtoms(buf, traf.offset + traf.headerSize, traf.offset + traf.size, "tfdt", tfdts);
        if (tfhds.empty()) continue;

        uint64_t tfhdLen = 0;
        const uint8_t* tfhd = atomPayloadPtr(buf, tfhds.front(), tfhdLen);
        if (!tfhd || tfhdLen < 8) continue;
        const uint32_t tfhdFlags = be32(tfhd) & 0x00FFFFFFu;
        const uint32_t trackId = be32(tfhd + 4);
        if (trackId != videoTrackId) continue;

        if (!tfdts.empty()) {
            uint64_t tlen = 0;
            const uint8_t* tp = atomPayloadPtr(buf, tfdts.front(), tlen);
            if (tp && tlen >= 8) {
                const uint8_t ver = tp[0];
                if (ver == 1 && tlen >= 12)
                    out.tfdt = be64(tp + 4);
                else
                    out.tfdt = be32(tp + 4);
                out.hasTfdt = true;
            }
        }

        uint64_t baseDataOffset = moof.offset; // 缺省：相对 moof（旧语义）
        uint32_t defaultSampleSize = trex ? trex->defaultSampleSize : 0;
        uint32_t defaultSampleDuration = trex ? trex->defaultSampleDuration : 0;
        size_t o = 8;
        if (tfhdFlags & 0x000001u) {
            if (o + 8 > tfhdLen) continue;
            baseDataOffset = be64(tfhd + o);
            o += 8;
        }
        if (tfhdFlags & 0x000002u) {
            if (o + 4 > tfhdLen) continue;
            o += 4; // sample-description-index
        }
        if (tfhdFlags & 0x000008u) {
            if (o + 4 > tfhdLen) continue;
            defaultSampleDuration = be32(tfhd + o);
            o += 4;
        }
        if (tfhdFlags & 0x000010u) {
            if (o + 4 > tfhdLen) continue;
            defaultSampleSize = be32(tfhd + o);
            o += 4;
        }
        if (tfhdFlags & 0x000020u) {
            if (o + 4 > tfhdLen) continue;
            o += 4; // default-sample-flags
        }
        if (tfhdFlags & 0x020000u) {
            baseDataOffset = moof.offset; // default-base-is-moof
        }

        for (const auto& trunAtom : truns) {
            uint64_t trunLen = 0;
            const uint8_t* trun = atomPayloadPtr(buf, trunAtom, trunLen);
            if (!trun || trunLen < 8) continue;
            const uint32_t trunFlags = be32(trun) & 0x00FFFFFFu;
            const uint32_t sampleCount = be32(trun + 4);
            size_t to = 8;
            int32_t dataOffset = 0;
            if (trunFlags & 0x000001u) {
                if (to + 4 > trunLen) continue;
                dataOffset = be32s(trun + to);
                to += 4;
            }
            if (trunFlags & 0x000004u) {
                if (to + 4 > trunLen) continue;
                to += 4; // first-sample-flags
            }

            uint64_t cur = static_cast<uint64_t>(static_cast<int64_t>(baseDataOffset) +
                                                  static_cast<int64_t>(dataOffset));
            for (uint32_t i = 0; i < sampleCount; ++i) {
                uint32_t sampleSize = defaultSampleSize;
                uint32_t sampleDuration = defaultSampleDuration;
                if (trunFlags & 0x000100u) {
                    if (to + 4 > trunLen) return false;
                    sampleDuration = be32(trun + to);
                    to += 4;
                }
                if (trunFlags & 0x000200u) {
                    if (to + 4 > trunLen) return false;
                    sampleSize = be32(trun + to);
                    to += 4;
                }
                if (trunFlags & 0x000400u) {
                    if (to + 4 > trunLen) return false;
                    to += 4; // sample-flags
                }
                if (trunFlags & 0x000800u) {
                    if (to + 4 > trunLen) return false;
                    to += 4; // composition-time-offset
                }
                if (sampleDuration > 0) {
                    out.durationTicks += sampleDuration;
                    out.hasDuration = true;
                }
                if (sampleSize == 0) continue;
                if (cur + sampleSize > static_cast<uint64_t>(buf.size())) return false;
                out.samples.emplace_back(cur, sampleSize);
                cur += sampleSize;
            }
        }
    }
    return true;
}

bool demuxFragments(const QByteArray& buf, uint32_t videoTrackId,
                    const TrexDefaults* trex, int lengthSize, uint32_t timescale,
                    DemuxResult& r) {
    const auto* fileData = reinterpret_cast<const uint8_t*>(buf.constData());
    const uint64_t fileSize = static_cast<uint64_t>(buf.size());
    uint64_t pos = 0;
    size_t sampleTotal = 0;
    uint64_t durationTicks = 0;
    bool hasDuration = false;
    uint64_t firstTfdt = 0;
    uint64_t lastTfdt = 0;
    size_t tfdtCount = 0;

    while (pos + 8 <= fileSize) {
        Atom a;
        uint64_t next = pos;
        if (!readAtom(buf, next, a)) break;
        if (std::strcmp(a.type, "moof") == 0) {
            FragExtractResult frag;
            if (!extractFragmentSamples(buf, a, videoTrackId, trex, frag)) {
                r.error = QStringLiteral("解析 moof/trun 失败（偏移 %1）").arg(a.offset);
                return false;
            }
            for (const auto& s : frag.samples) {
                auto nals = NalParser::parseAvcc(fileData + s.first, s.second, lengthSize, s.first);
                r.nals.insert(r.nals.end(), nals.begin(), nals.end());
            }
            sampleTotal += frag.samples.size();
            if (frag.hasDuration) {
                durationTicks += frag.durationTicks;
                hasDuration = true;
            }
            if (frag.hasTfdt) {
                if (tfdtCount == 0) firstTfdt = frag.tfdt;
                lastTfdt = frag.tfdt;
                ++tfdtCount;
            }
        }
        pos = a.offset + a.size;
        if (a.size == 0) break;
    }

    if (sampleTotal == 0) {
        r.error = QStringLiteral("fMP4 中未找到视频样本（track %1）").arg(videoTrackId);
        return false;
    }

    // 帧率：优先样本时长合计；否则用 tfdt 跨度估算（补一段平均 fragment 时长）
    if (timescale > 0 && hasDuration && durationTicks > 0) {
        const double seconds = static_cast<double>(durationTicks) / static_cast<double>(timescale);
        if (seconds > 0.0)
            r.fps = static_cast<double>(sampleTotal) / seconds;
    } else if (timescale > 0 && tfdtCount >= 2 && lastTfdt > firstTfdt) {
        uint64_t span = lastTfdt - firstTfdt;
        const uint64_t avgFrag = span / (tfdtCount - 1);
        span += avgFrag;
        const double seconds = static_cast<double>(span) / static_cast<double>(timescale);
        if (seconds > 0.0)
            r.fps = static_cast<double>(sampleTotal) / seconds;
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

    const uint64_t fileSize = static_cast<uint64_t>(buf.size());

    AvcCConfig avcCfg;
    bool foundAvc = false;
    std::vector<Atom> stsdAtoms;
    findAtoms(buf, 0, fileSize, "stsd", stsdAtoms);
    for (const auto& stsd : stsdAtoms) {
        const QByteArray payload = atomPayload(buf, stsd);
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

    const auto tracks = collectTracks(buf);
    const TrackInfo* video = nullptr;
    for (const auto& t : tracks) {
        if (t.isVideo) {
            video = &t;
            break;
        }
    }
    if (!video) {
        r.error = QStringLiteral("未找到 vide 轨道");
        return r;
    }

    if (video->timescale > 0)
        r.fps = static_cast<double>(video->timescale); // 暂存，后面用样本数修正

    std::vector<std::pair<uint32_t, TrexDefaults>> trexList;
    parseTrexMap(buf, trexList);
    const TrexDefaults* trex = findTrex(trexList, video->trackId);

    std::vector<Atom> mvexAtoms, moofAtoms;
    findAtoms(buf, 0, fileSize, "mvex", mvexAtoms);
    findAtoms(buf, 0, fileSize, "moof", moofAtoms);

    // 仅在 vide trak 内取样本表，避免音轨 .front() 误绑
    std::vector<uint32_t> sampleSizes;
    std::vector<uint64_t> chunkOffsets;
    std::vector<StscEntry> stsc;
    {
        std::vector<Atom> stszAtoms, stcoAtoms, co64Atoms, stscAtoms;
        const uint64_t tb = video->trak.offset + video->trak.headerSize;
        const uint64_t te = video->trak.offset + video->trak.size;
        findAtoms(buf, tb, te, "stsz", stszAtoms);
        findAtoms(buf, tb, te, "stco", stcoAtoms);
        findAtoms(buf, tb, te, "co64", co64Atoms);
        findAtoms(buf, tb, te, "stsc", stscAtoms);
        if (!stszAtoms.empty())
            parseStsz(buf, stszAtoms.front(), sampleSizes);
        if (!stcoAtoms.empty())
            parseChunkOffsets(buf, &stcoAtoms.front(), nullptr, chunkOffsets);
        else if (!co64Atoms.empty())
            parseChunkOffsets(buf, nullptr, &co64Atoms.front(), chunkOffsets);
        if (!stscAtoms.empty())
            parseStsc(buf, stscAtoms.front(), stsc);
    }

    const bool hasProgressiveSamples =
        !sampleSizes.empty() && !chunkOffsets.empty() && !stsc.empty();
    const bool isFragmented = !mvexAtoms.empty() || !moofAtoms.empty();

    if (!hasProgressiveSamples && isFragmented) {
        if (!demuxFragments(buf, video->trackId, trex, avcCfg.lengthSize, video->timescale, r))
            return r;
        if (r.fps > 1000.0 || r.fps <= 0.0)
            r.fps = 25.0;
        return r;
    }

    if (!hasProgressiveSamples) {
        // 旧回退：仅第一个 mdat（非 fMP4 且表空时）
        std::vector<Atom> mdats;
        findAtoms(buf, 0, fileSize, "mdat", mdats);
        if (!mdats.empty()) {
            uint64_t mlen = 0;
            const uint8_t* mp = atomPayloadPtr(buf, mdats.front(), mlen);
            if (mp && mlen > 0) {
                auto more = NalParser::parseAvcc(mp, static_cast<size_t>(mlen), avcCfg.lengthSize,
                                                 mdats.front().offset + mdats.front().headerSize);
                r.nals.insert(r.nals.end(), more.begin(), more.end());
            }
        }
        if (r.fps > 1000) r.fps = 25.0;
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
            if (off + sz > fileSize) break;
            auto nals = NalParser::parseAvcc(fileData + off, sz, avcCfg.lengthSize, off);
            r.nals.insert(r.nals.end(), nals.begin(), nals.end());
            off += sz;
        }
    }

    if (video->timescale > 0 && video->duration > 0 && !sampleSizes.empty()) {
        const double seconds = static_cast<double>(video->duration) / static_cast<double>(video->timescale);
        if (seconds > 0.0)
            r.fps = static_cast<double>(sampleSizes.size()) / seconds;
        else
            r.fps = 25.0;
    } else if (r.fps > 100.0 && !sampleSizes.empty()) {
        r.fps = 25.0;
    } else if (r.fps <= 0.0) {
        r.fps = 25.0;
    }

    return r;
}

} // namespace h264tool
