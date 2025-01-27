#include "FFmpegStreamWriter.hpp"
#include <iostream>

namespace bsp_container
{

FFmpegStreamWriter::~FFmpegStreamWriter()
{
    closeStreamWriter();
}

int FFmpegStreamWriter::openStreamWriter(const std::string &filename, std::any params)
{
    AVCodecParameters* codecParams = std::any_cast<AVCodecParameters*>(params);
    if (codecParams == nullptr)
    {
        std::cerr << "Codec parameters not provided." << std::endl;
        return -1;
    }

    AVFormatContext *format_Ctx = nullptr;
    if (avformat_alloc_output_context2(&format_Ctx, nullptr, nullptr, filename.c_str()) < 0)
    {
        std::cerr << "Could not allocate output context." << std::endl;
        return -1;
    }

    m_format_Ctx = std::shared_ptr<AVFormatContext>(format_Ctx, [](AVFormatContext *p) { avformat_free_context(p); });

    AVStream *out_stream = avformat_new_stream(m_format_Ctx.get(), nullptr);
    if (out_stream == nullptr)
    {
        std::cerr << "Could not allocate output stream." << std::endl;
        m_format_Ctx.reset();
        return -1;
    }

    if (avcodec_parameters_copy(out_stream->codecpar, codecParams) < 0)
    {
        std::cerr << "Could not copy codec parameters." << std::endl;
        m_format_Ctx.reset();
        return -1;
    }

    if (avio_open(&m_format_Ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0)
    {
        std::cerr << "Could not open output file." << std::endl;
        m_format_Ctx.reset();
        return -1;
    }

    return 0;
}

void FFmpegStreamWriter::closeStreamWriter()
{
    if (m_format_Ctx == nullptr)
    {
        return;
    }
    avio_close(m_format_Ctx->pb);
    m_format_Ctx.reset();
}

int FFmpegStreamWriter::writeHeader()
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Output context not allocated." << std::endl;
        return -1;
    }

    if (avformat_write_header(m_format_Ctx.get(), nullptr) < 0)
    {
        std::cerr << "Could not write header." << std::endl;
        return -1;
    }

    return 0;
}

int FFmpegStreamWriter::writePacket(const StreamPacket &packet)
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Output context not allocated." << std::endl;
        return -1;
    }

    AVPacket pkt;
    pkt.data = const_cast<uint8_t*>(packet.pkt_data.data());
    pkt.size = packet.pkt_size;
    pkt.pts = packet.pts;
    pkt.dts = packet.dts;
    pkt.stream_index = packet.stream_index;

    if (av_interleaved_write_frame(m_format_Ctx.get(), &pkt) < 0)
    {
        std::cerr << "Could not write frame." << std::endl;
        return -1;
    }
    return 0;
}

int FFmpegStreamWriter::writeTrailer(const uint8_t *data, size_t size)
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Output context not allocated." << std::endl;
        return -1;
    }

    if (av_write_trailer(m_format_Ctx.get()) < 0)
    {
        std::cerr << "Could not write trailer." << std::endl;
        return -1;
    }

    return 0;
}

} // namespace bsp_container