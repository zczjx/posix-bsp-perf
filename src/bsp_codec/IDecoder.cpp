#include <bsp_codec/IDecoder.hpp>
#include "video/decoder/rkmppDec.hpp"
#include <stdexcept>
#include <memory>

namespace bsp_codec
{
static std::unique_ptr<IDecoder> create(const std::string& codecPlatform)
{
    if (codecPlatform.compare("rkmpp") == 0)
    {
        return std::make_unique<rkmppDec>();
    }
    else
    {
        throw std::invalid_argument("Invalid Decoder platform specified.");
    }
}

} // namespace bsp_codec