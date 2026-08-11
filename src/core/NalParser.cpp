#include "core/NalParser.h"

namespace h264tool {
namespace {

bool isStartCode(const uint8_t* p, size_t remain, int& scSize) {
    if (remain >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
        scSize = 4;
        return true;
    }
    if (remain >= 3 && p[0] == 0 && p[1] == 0 && p[2] == 1) {
        scSize = 3;
        return true;
    }
    return false;
}

/** 查找下一个起始码位置，返回相对偏移；找不到返回 remain */
size_t findNextStartCode(const uint8_t* data, size_t from, size_t size) {
    for (size_t i = from; i + 3 <= size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0) {
            if (data[i + 2] == 1) return i;
            if (i + 3 < size && data[i + 2] == 0 && data[i + 3] == 1) return i;
        }
    }
    return size;
}

uint32_t readBe(const uint8_t* p, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i)
        v = (v << 8) | p[i];
    return v;
}

} // namespace

void NalParser::fillHeaderFields(NalUnit& nal) {
    if (nal.payload.empty()) {
        nal.type = NalType::Unknown;
        return;
    }
    const uint8_t b = nal.payload[0];
    nal.nalRefIdc = static_cast<uint8_t>((b >> 5) & 0x3);
    nal.nalUnitTypeId = static_cast<uint8_t>(b & 0x1F);
    nal.type = nalTypeFromId(nal.nalUnitTypeId);
}

std::vector<NalUnit> NalParser::parseAnnexB(const uint8_t* data, size_t size, uint64_t baseOffset) {
    std::vector<NalUnit> out;
    if (!data || size == 0) return out;

    size_t i = 0;
    // 跳过前导零直到起始码
    while (i < size) {
        int scSize = 0;
        if (!isStartCode(data + i, size - i, scSize)) {
            ++i;
            continue;
        }
        const size_t nalStart = i;
        const size_t payloadStart = i + static_cast<size_t>(scSize);
        const size_t next = findNextStartCode(data, payloadStart, size);
        NalUnit nal;
        nal.fileOffset = baseOffset + nalStart;
        nal.headerSize = static_cast<uint32_t>(scSize);
        nal.payload.assign(data + payloadStart, data + next);
        fillHeaderFields(nal);
        out.push_back(std::move(nal));
        i = next;
    }
    return out;
}

std::vector<NalUnit> NalParser::parseAvcc(const uint8_t* data, size_t size,
                                          int lengthSize, uint64_t baseOffset) {
    std::vector<NalUnit> out;
    if (!data || size == 0 || lengthSize < 1 || lengthSize > 4) return out;

    size_t i = 0;
    while (i + static_cast<size_t>(lengthSize) <= size) {
        const uint32_t nalLen = readBe(data + i, lengthSize);
        const size_t headerOff = i;
        i += static_cast<size_t>(lengthSize);
        if (nalLen == 0 || i + nalLen > size) break;

        NalUnit nal;
        nal.fileOffset = baseOffset + headerOff;
        nal.headerSize = static_cast<uint32_t>(lengthSize);
        nal.payload.assign(data + i, data + i + nalLen);
        fillHeaderFields(nal);
        out.push_back(std::move(nal));
        i += nalLen;
    }
    return out;
}

} // namespace h264tool
