#include "IEncoder.hpp"
#ifdef BUILD_PLATFORM_RK35XX
#include "video/rkmppEnc.hpp"
#endif
#ifdef BUILD_PLATFORM_JETSON
#include "video/nvVideoEnc.hpp"
#endif
#include <stdexcept>
#include <memory>

namespace bsp_codec
{
std::unique_ptr<IEncoder> IEncoder::create(const std::string& codecPlatform)
{
#ifdef BUILD_PLATFORM_RK35XX
    if (codecPlatform.compare("rkmpp") == 0)
    {
        return std::make_unique<rkmppEnc>();
    }
#endif
#ifdef BUILD_PLATFORM_JETSON
    if (codecPlatform.compare("nvenc") == 0)
    {
        return std::make_unique<nvVideoEnc>();
    }
#endif
    throw std::invalid_argument("Invalid Encoder platform specified.");
}

} // namespace bsp_codec