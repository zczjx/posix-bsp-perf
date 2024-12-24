#include <bsp_codec/IDecoder.hpp>
#include "video/rkmppDec.hpp"
#include <stdexcept>
#include <memory>

namespace bsp_codec
{
std::unique_ptr<IDecoder> IDecoder::create(const std::string& codecPlatform)
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