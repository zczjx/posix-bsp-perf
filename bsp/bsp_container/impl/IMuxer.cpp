#include <bsp_container/IMuxer.hpp>
#include "ffmpeg/FFmpegMuxer.hpp"
#include <stdexcept>
#include <memory>
namespace bsp_container
{
std::unique_ptr<IMuxer> IMuxer::create(const std::string& containerPlatform)
{
    if (containerPlatform.compare("FFmpegMuxer") == 0)
    {
        return std::make_unique<FFmpegMuxer>();
    }
    else
    {
        throw std::invalid_argument("Invalid Muxer platform specified.");
    }
}

} // namespace bsp_container