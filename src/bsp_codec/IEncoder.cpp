#include "IEncoder.hpp"
#include "video/encoder/rkmppEnc.hpp"
#include <stdexcept>
#include <memory>

namespace bsp_codec
{
std::unique_ptr<IEncoder> IEncoder::create(const std::string& codecPlatform)
{
    if (codecPlatform.compare("rkmpp") == 0)
    {
        return std::make_unique<rkmppEnc>();
    }
    else
    {
        throw std::invalid_argument("Invalid Encoder platform specified.");
    }
}

} // namespace bsp_codec