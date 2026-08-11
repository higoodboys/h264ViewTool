#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace h264tool {

/**
 * @brief 按位读取器，实现 H.264 常用的 u(n) / ue(v) / se(v)。
 *
 * 用于 SPS、PPS、Slice Header 等无起始码防竞争字节的已剥离 RBSP。
 */
class BitReader {
public:
    BitReader() = default;
    explicit BitReader(const std::vector<uint8_t>& data)
        : data_(data.data()), size_(data.size()) {}
    BitReader(const uint8_t* data, size_t size)
        : data_(data), size_(size) {}

    size_t bitsRemaining() const {
        if (bitPos_ >= size_ * 8) return 0;
        return size_ * 8 - bitPos_;
    }

    bool exhausted() const { return bitsRemaining() == 0; }

    /** 读取 n 位无符号整数 u(n) */
    uint32_t readBits(int n) {
        if (n < 0 || n > 32) throw std::runtime_error("readBits: invalid n");
        if (static_cast<size_t>(n) > bitsRemaining())
            throw std::runtime_error("readBits: underflow");
        uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            const size_t byteIndex = bitPos_ / 8;
            const int shift = 7 - static_cast<int>(bitPos_ % 8);
            const uint32_t bit = (data_[byteIndex] >> shift) & 0x1u;
            v = (v << 1) | bit;
            ++bitPos_;
        }
        return v;
    }

    bool readFlag() { return readBits(1) != 0; }

    /**
     * 无符号指数哥伦布码 ue(v)
     * codeNum = 2^leadingZeroBits - 1 + read_bits(leadingZeroBits)
     */
    uint32_t readUe() {
        int leadingZeros = 0;
        while (readBits(1) == 0) {
            ++leadingZeros;
            if (leadingZeros > 31)
                throw std::runtime_error("readUe: too many leading zeros");
        }
        if (leadingZeros == 0) return 0;
        return static_cast<uint32_t>((1u << leadingZeros) - 1u + readBits(leadingZeros));
    }

    /** 有符号指数哥伦布码 se(v) */
    int32_t readSe() {
        const uint32_t codeNum = readUe();
        const int32_t absVal = static_cast<int32_t>((codeNum + 1) / 2);
        return (codeNum % 2 == 0) ? -absVal : absVal;
    }

    void skipBits(int n) { (void)readBits(n); }

    size_t bitPosition() const { return bitPos_; }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t bitPos_ = 0;
};

/**
 * @brief 将 EBSP（含防竞争字节 0x03）转换为 RBSP。
 */
inline std::vector<uint8_t> ebspToRbsp(const uint8_t* data, size_t size) {
    std::vector<uint8_t> out;
    out.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        if (i + 2 < size && data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x03) {
            out.push_back(0x00);
            out.push_back(0x00);
            i += 2; // 跳过 0x03
            continue;
        }
        out.push_back(data[i]);
    }
    return out;
}

} // namespace h264tool
