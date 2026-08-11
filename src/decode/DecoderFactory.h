#pragma once

#include "decode/IDecoder.h"

#include <memory>

namespace h264tool {

class DecoderFactory {
public:
    /** 优先 FFmpeg，否则 NullDecoder */
    static std::unique_ptr<IDecoder> create();
};

} // namespace h264tool
