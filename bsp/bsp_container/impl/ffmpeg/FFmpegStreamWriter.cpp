#include "FFmpegStreamWriter.hpp"
#include <iostream>
#include <cstring>

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
    m_packet.reset();
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

    if (m_packet == nullptr)
    {
        AVPacket* tmp_packet = av_packet_alloc();
        m_packet = std::shared_ptr<AVPacket>(tmp_packet, [](AVPacket* p) { av_packet_free(&p); });
        av_new_packet(m_packet.get(), packet.useful_pkt_size);
    }

    if (m_packet->size < packet.useful_pkt_size)
    {
        av_grow_packet(m_packet.get(), packet.useful_pkt_size);
        m_packet->size = packet.useful_pkt_size;
    }

    std::memcpy(m_packet->data, packet.pkt_data.data(), m_packet->size);
    m_packet->pts = packet.pts;
    m_packet->dts = packet.dts;
    std::cerr << "FFmpegStreamWriter::writePacket: m_packet->pts: " << m_packet->pts << std::endl;
    std::cerr << "FFmpegStreamWriter::writePacket: m_packet->dts: " << m_packet->dts << std::endl;
    m_packet->stream_index = packet.stream_index;
    m_packet->pos = packet.pos;
    m_packet->time_base.num = 1;
    m_packet->time_base.den  = 30;

    if (av_interleaved_write_frame(m_format_Ctx.get(), m_packet.get()) < 0)
    {
        std::cerr << "Could not write frame." << std::endl;
        return -1;
    }
    return 0;
}

int FFmpegStreamWriter::writeTrailer()
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