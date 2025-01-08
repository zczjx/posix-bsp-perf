#include "FFmpegDemuxer.hpp"
#include <iostream>

namespace bsp_container
{

FFmpegDemuxer::~FFmpegDemuxer()
{
    closeContainerParser();
}

int FFmpegDemuxer::openContainerParser(const std::string& path)
{
    AVFormatContext* format_Ctx = nullptr;

    if (avformat_open_input(&format_Ctx, path.c_str(), nullptr, nullptr) < 0)
    {
        std::cerr << "Could not open input file: " << path << std::endl;
        return -1;
    }

    m_format_Ctx = std::shared_ptr<AVFormatContext>(format_Ctx, [](AVFormatContext* p) { avformat_close_input(&p); });

    if (avformat_find_stream_info(m_format_Ctx.get(), nullptr) < 0)
    {
        std::cerr << "Could not find stream information." << std::endl;
        return -1;
    }

    return 0;
}

void FFmpegDemuxer::closeContainerParser()
{
    m_format_Ctx.reset();
}

int FFmpegDemuxer::getContainerInfo(ContainerInfo& containerInfo)
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Container not opened." << std::endl;
        return -1;
    }

    return 0;
}


} // namespace bsp_container