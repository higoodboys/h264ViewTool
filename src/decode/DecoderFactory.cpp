#include "decode/DecoderFactory.h"

#include "decode/NullDecoder.h"

#if H264TOOL_HAS_FFMPEG
#include "decode/FFmpegDecoder.h"
#endif

namespace h264tool {

std::unique_ptr<IDecoder> DecoderFactory::create() {
#if H264TOOL_HAS_FFMPEG
    return std::make_unique<FFmpegDecoder>();
#else
    return std::make_unique<NullDecoder>();
#endif
}

} // namespace h264tool
