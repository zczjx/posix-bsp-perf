#include <bsp_codec/IDecoder.hpp>
#ifdef BUILD_PLATFORM_RK35XX
#include "video/rkmppDec.hpp"
#endif
#ifdef BUILD_PLATFORM_JETSON
#include "video/nvVideoDec.hpp"
#endif
#include <stdexcept>
#include <memory>

namespace bsp_codec
{
std::unique_ptr<IDecoder> IDecoder::create(const std::string& codecPlatform)
{
#ifdef BUILD_PLATFORM_RK35XX
    if (codecPlatform.compare("rkmpp") == 0)
    {
        return std::make_unique<rkmppDec>();
    }
#endif
#ifdef BUILD_PLATFORM_JETSON
    if (codecPlatform.compare("nvdec") == 0)
    {
        return std::make_unique<nvVideoDec>();
    }
#endif
    throw std::invalid_argument("Invalid Decoder platform specified.");
}

} // namespace bsp_codec