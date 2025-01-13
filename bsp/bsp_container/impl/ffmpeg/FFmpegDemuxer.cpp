#include "FFmpegDemuxer.hpp"
#include <iostream>
#include <cstring>
#include <libavutil/dict.h>

namespace bsp_container
{

FFmpegDemuxer::~FFmpegDemuxer()
{
    closeContainerDemux();
}

int FFmpegDemuxer::openContainerDemux(const std::string& path)
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

void FFmpegDemuxer::closeContainerDemux()
{
    m_packet.reset();
    m_format_Ctx.reset();
}

int FFmpegDemuxer::getContainerInfo(ContainerInfo& containerInfo)
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Container not opened." << std::endl;
        return -1;
    }

    containerInfo.container_type = m_format_Ctx->iformat->name;

    const AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_iterate(m_format_Ctx->metadata, tag)))
    {
        containerInfo.metadata[tag->key] = tag->value;
    }

    containerInfo.num_of_streams = m_format_Ctx->nb_streams;
    for (int i = 0; i < containerInfo.num_of_streams; i++)
    {
        StreamInfo streamInfo;
        streamInfo.index = m_format_Ctx->streams[i]->index;
        streamInfo.codec_params.codec_type = av_get_media_type_string(m_format_Ctx->streams[i]->codecpar->codec_type);
        const AVDictionaryEntry* tag = nullptr;
        while ((tag = av_dict_iterate(m_format_Ctx->streams[i]->metadata, tag)))
        {
            streamInfo.metadata[tag->key] = tag->value;
        }

        getStreamDisposition(m_format_Ctx->streams[i], streamInfo.disposition_list);
        streamInfo.start_time = m_format_Ctx->streams[i]->start_time;
        streamInfo.duration = m_format_Ctx->streams[i]->duration;
        streamInfo.num_of_frames = m_format_Ctx->streams[i]->nb_frames;
        streamInfo.codec_params.sample_aspect_ratio = av_q2d(m_format_Ctx->streams[i]->sample_aspect_ratio);
        streamInfo.codec_params.frame_rate = av_q2d(m_format_Ctx->streams[i]->r_frame_rate);
        containerInfo.stream_info_list.push_back(streamInfo);
    }
    containerInfo.bit_rate = m_format_Ctx->bit_rate;
    containerInfo.packet_size = m_format_Ctx->packet_size;

    return 0;
}

void FFmpegDemuxer::getStreamDisposition(const AVStream* stream, std::vector<std::string>& disposition_list)
{
    if (stream->disposition & AV_DISPOSITION_DEFAULT)
    {
        disposition_list.push_back("default");
    }
    if (stream->disposition & AV_DISPOSITION_DUB)
    {
        disposition_list.push_back("dub");
    }
    if (stream->disposition & AV_DISPOSITION_ORIGINAL)
    {
        disposition_list.push_back("original");
    }
    if (stream->disposition & AV_DISPOSITION_COMMENT)
    {
        disposition_list.push_back("comment");
    }
    if (stream->disposition & AV_DISPOSITION_LYRICS)
    {
        disposition_list.push_back("lyrics");
    }
    if (stream->disposition & AV_DISPOSITION_KARAOKE)
    {
        disposition_list.push_back("karaoke");
    }
    if (stream->disposition & AV_DISPOSITION_FORCED)
    {
        disposition_list.push_back("forced");
    }
    if (stream->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
    {
        disposition_list.push_back("hearing_impaired");
    }
    if (stream->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
    {
        disposition_list.push_back("visual_impaired");
    }
    if (stream->disposition & AV_DISPOSITION_CLEAN_EFFECTS)
    {
        disposition_list.push_back("clean_effects");
    }
    if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC)
    {
        disposition_list.push_back("attached_pic");
    }
    if (stream->disposition & AV_DISPOSITION_TIMED_THUMBNAILS)
    {
        disposition_list.push_back("timed_thumbnails");
    }
    if (stream->disposition & AV_DISPOSITION_CAPTIONS)
    {
        disposition_list.push_back("captions");
    }
    if (stream->disposition & AV_DISPOSITION_DESCRIPTIONS)
    {
        disposition_list.push_back("descriptions");
    }
    if (stream->disposition & AV_DISPOSITION_METADATA)
    {
        disposition_list.push_back("metadata");
    }
    if (stream->disposition & AV_DISPOSITION_DEPENDENT)
    {
        disposition_list.push_back("dependent");
    }
    if (stream->disposition & AV_DISPOSITION_STILL_IMAGE)
    {
        disposition_list.push_back("still_image");
    }

    if (stream->disposition & AV_DISPOSITION_NON_DIEGETIC)
    {
        disposition_list.push_back("non_diegetic");
    }
}

int FFmpegDemuxer::readStreamPacket(StreamPacket& streamPacket)
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Container not opened." << std::endl;
        return -1;
    }

    if(m_packet == nullptr)
    {
        m_packet = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); });
    }

    if (av_read_frame(m_format_Ctx.get(), m_packet.get()) < 0)
    {
        std::cerr << "Could not read frame." << std::endl;
        return -1;
    }

    streamPacket.stream_index = m_packet->stream_index;
    streamPacket.pts = m_packet->pts;
    streamPacket.dts = m_packet->dts;
    streamPacket.pkt_size = m_packet->size;
    if(streamPacket.pkt_data.size() < streamPacket.pkt_size)
    {
        streamPacket.pkt_data.resize(streamPacket.pkt_size);
    }
    std::memcpy(streamPacket.pkt_data.data(), m_packet->data, streamPacket.pkt_size);
    streamPacket.pos = m_packet->pos;
    return 0;
}

int FFmpegDemuxer::seekStreamFrame(int stream_index, int64_t timestamp)
{
    return 0;
}


} // namespace bsp_container