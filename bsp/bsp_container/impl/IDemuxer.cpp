#include <bsp_container/IDemuxer.hpp>
#include "ffmpeg/FFmpegDemuxer.hpp"
#include <stdexcept>
#include <memory>
namespace bsp_container
{
std::unique_ptr<IDemuxer> IDemuxer::create(const std::string& containerPlatform)
{
    if (containerPlatform.compare("ffmpeg") == 0)
    {
        return std::make_unique<FFmpegDemuxer>();
    }
    else
    {
        throw std::invalid_argument("Invalid Demuxer platform specified.");
    }
}

} // namespace bsp_container