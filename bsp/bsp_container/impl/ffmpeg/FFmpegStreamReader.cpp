#include "FFmpegStreamReader.hpp"
#include <iostream>
#include <cstring>

namespace bsp_container
{

FFmpegStreamReader::~FFmpegStreamReader()
{
    closeStreamReader();
}

int FFmpegStreamReader::openStreamReader(const std::string& filename)
{
    AVFormatContext *format_Ctx = nullptr;

    if (avformat_open_input(&format_Ctx, filename.c_str(), nullptr, nullptr) < 0)
    {
        std::cerr << "Could not open input file." << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(format_Ctx, nullptr) < 0)
    {
        std::cerr << "Could not find stream information." << std::endl;
        avformat_close_input(&format_Ctx);
        return -1;
    }

    av_dump_format(format_Ctx, 0, filename.c_str(), 0);

    m_format_Ctx = std::shared_ptr<AVFormatContext>(format_Ctx, [](AVFormatContext *p) { avformat_close_input(&p); });

}

void FFmpegStreamReader::closeStreamReader()
{
    if (m_format_Ctx == nullptr)
    {
        return;
    }

    m_format_Ctx.reset();
}

std::any FFmpegStreamReader::getStreamParams()
{
    AVStream *in_stream = m_format_Ctx->streams[0];

    std::shared_ptr<AVCodecParameters> codecParams{nullptr};

    codecParams.reset(in_stream->codecpar);

    return codecParams;
}

int FFmpegStreamReader::readPacket(StreamPacket& packet)
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Input context not allocated." << std::endl;
        return -1;
    }

    if (m_packet == nullptr)
    {
        AVPacket* tmp_packet = av_packet_alloc();
        m_packet = std::shared_ptr<AVPacket>(tmp_packet, [](AVPacket* p) { av_packet_free(&p); });
    }

    int ret = av_read_frame(m_format_Ctx.get(), m_packet.get());

    if (ret < 0)
    {
        std::cerr << "Could not read frame." << std::endl;
        return -1;
    }

    packet.stream_index = m_packet->stream_index;
    packet.pts = m_packet->pts;
    packet.dts = m_packet->dts;
    packet.useful_pkt_size = m_packet->size;
    if (packet.pkt_data.size() < packet.useful_pkt_size)
    {
        packet.pkt_data.resize(packet.useful_pkt_size);
    }
    std::memcpy(packet.pkt_data.data(), m_packet->data, packet.useful_pkt_size);
    packet.pos = m_packet->pos;
    return ret;
}

}