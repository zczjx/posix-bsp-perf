#include "FFmpegMuxer.hpp"
#include <iostream>
#include <cstring>
#include <libavutil/dict.h>
namespace bsp_container
{

FFmpegMuxer::~FFmpegMuxer()
{
    closeContainerMux();
}

int FFmpegMuxer::openContainerMux(const std::string& path)
{
    AVFormatContext* format_Ctx = nullptr;
    avformat_alloc_output_context2(&format_Ctx, nullptr, nullptr, path.c_str());
    if (format_Ctx == nullptr)
    {
        std::cerr << "Could not allocate output context." << std::endl;
        return -1;
    }

    m_format_Ctx = std::shared_ptr<AVFormatContext>(format_Ctx, [](AVFormatContext* p) { avformat_free_context(p); });

}

void FFmpegMuxer::closeContainerMux()
{
    m_packet.reset();
    m_format_Ctx.reset();
}

int FFmpegMuxer::addStream(StreamInfo& streamInfo)
{
    AVStream* out_stream = avformat_new_stream(m_format_Ctx.get(), nullptr);
    if (out_stream == nullptr)
    {
        std::cerr << "Could not allocate output stream." << std::endl;
        return -1;
    }

}

int FFmpegMuxer::writeStreamPacket(StreamPacket& streamPacket)
{

}


} // namespace bsp_container