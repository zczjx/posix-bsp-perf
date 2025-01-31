#include "FFmpegMuxer.hpp"
#include "ffmpegCodecHeader.hpp"
#include "FFmpegStreamReader.hpp"
#include <iostream>
#include <cstring>
extern "C" {
#include <libavutil/dict.h>
}
namespace bsp_container
{

FFmpegMuxer::~FFmpegMuxer()
{
    closeContainerMux();
}

int FFmpegMuxer::openContainerMux(const std::string& path, MuxConfig& config)
{
    int ret = 0;
    AVFormatContext* format_Ctx = nullptr;
    avformat_alloc_output_context2(&format_Ctx, nullptr, nullptr, path.c_str());
    if (format_Ctx == nullptr)
    {
        std::cerr << "Could not allocate output context." << std::endl;
        return -1;
    }

    m_format_Ctx = std::shared_ptr<AVFormatContext>(format_Ctx, [](AVFormatContext* p) { avformat_free_context(p); });
    m_path = path;
    m_mux_cfg.ts_recreate = config.ts_recreate;
    m_mux_cfg.video_fps = config.video_fps;
}

void FFmpegMuxer::closeContainerMux()
{
    if (m_format_Ctx == nullptr)
    {
        return;
    }

    if (!(m_format_Ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&m_format_Ctx->pb);
    }
    m_format_Ctx.reset();
}

void FFmpegMuxer::setupVideoStreamParams(AVStream* out_stream, StreamInfo& streamInfo)
{
    if (out_stream == nullptr)
    {
        std::cerr << "Output video sstream is null." << std::endl;
        return;
    }

    out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    out_stream->codecpar->codec_id = ffmpegCodecHeader::getInstance().strToFFmpegCoding(streamInfo.codec_params.codec_name);
    out_stream->codecpar->bit_rate = streamInfo.codec_params.bit_rate;
    out_stream->codecpar->sample_aspect_ratio = av_d2q(streamInfo.codec_params.sample_aspect_ratio, 255);
    out_stream->codecpar->width = streamInfo.codec_params.width;
    out_stream->codecpar->height = streamInfo.codec_params.height;
    out_stream->time_base = {1, streamInfo.codec_params.frame_rate};
}

void FFmpegMuxer::setupAudioStreamParams(AVStream* out_stream, StreamInfo& streamInfo)
{
    if (out_stream == nullptr)
    {
        std::cerr << "Output audio stream is null." << std::endl;
        return;
    }

    out_stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    out_stream->codecpar->codec_id = ffmpegCodecHeader::getInstance().strToFFmpegCoding(streamInfo.codec_params.codec_name);
    out_stream->codecpar->bit_rate = streamInfo.codec_params.bit_rate;
    out_stream->codecpar->sample_rate = streamInfo.codec_params.sample_rate;
    out_stream->time_base = {1, streamInfo.codec_params.sample_rate};

}

int FFmpegMuxer::addStream(StreamInfo& streamInfo)
{
    AVStream* out_stream = avformat_new_stream(m_format_Ctx.get(), nullptr);
    if (out_stream == nullptr)
    {
        std::cerr << "Could not allocate output stream." << std::endl;
        return -1;
    }

    out_stream->id = m_format_Ctx->nb_streams - 1;

    if (streamInfo.codec_params.codec_type == "video")
    {
        setupVideoStreamParams(out_stream, streamInfo);
    }
    else if (streamInfo.codec_params.codec_type == "audio")
    {
        setupAudioStreamParams(out_stream, streamInfo);
    }
    else
    {
        std::cerr << "Unsupported codec type." << std::endl;
        return -1;
    }

    m_stream_frames_count_map[out_stream->id] = 0;
    return out_stream->id;

}

int FFmpegMuxer::addStream(std::shared_ptr<StreamReader> strReader)
{
    std::shared_ptr<AVCodecParameters> codecParams = std::any_cast<std::shared_ptr<AVCodecParameters>>(strReader->getStreamParams());
    if (codecParams == nullptr)
    {
        std::cerr << "Codec parameters not provided." << std::endl;
        return -1;
    }

    AVStream *out_stream = avformat_new_stream(m_format_Ctx.get(), nullptr);
    if (out_stream == nullptr)
    {
        std::cerr << "Could not allocate output stream." << std::endl;
        return -1;
    }

    out_stream->id = m_format_Ctx->nb_streams - 1;

    if (avcodec_parameters_copy(out_stream->codecpar, codecParams.get()) < 0)
    {
        std::cerr << "Could not copy codec parameters." << std::endl;
        return -1;
    }

    out_stream->codecpar->codec_tag = 0;
    m_stream_frames_count_map[out_stream->id] = 0;
    return out_stream->id;
}

int64_t FFmpegMuxer::recreatePTS(int stream_index)
{
    if (m_stream_frames_count_map.find(stream_index) == m_stream_frames_count_map.end())
    {
        std::cerr << "Stream index not found." << std::endl;
        return -1;
    }

    AVStream * outstream = m_format_Ctx->streams[stream_index];

    float_t duration_secs = 1.0 / m_mux_cfg.video_fps;

    int64_t duration_ts = (duration_secs * outstream->time_base.den) / outstream->time_base.num;

    return m_stream_frames_count_map[stream_index] * duration_ts;
}

int64_t FFmpegMuxer::recreateDTS(int stream_index)
{
    if (m_stream_frames_count_map.find(stream_index) == m_stream_frames_count_map.end())
    {
        std::cerr << "Stream index not found." << std::endl;
        return -1;
    }

    AVStream * outstream = m_format_Ctx->streams[stream_index];

    float_t duration_secs = 1.0 / m_mux_cfg.video_fps;

    int64_t duration_ts = (duration_secs * outstream->time_base.den) / outstream->time_base.num;

    return (m_stream_frames_count_map[stream_index] - 1) * duration_ts;
}


int FFmpegMuxer::writeStreamPacket(StreamPacket& streamPacket)
{
    if (m_header_written == false)
    {
        if (m_format_Ctx == nullptr)
        {
            std::cerr << "Output format context is null." << std::endl;
            return -1;
        }

        if(!(m_format_Ctx->oformat->flags & AVFMT_NOFILE))
        {
            if (avio_open(&m_format_Ctx->pb, m_path.c_str(), AVIO_FLAG_WRITE) < 0)
            {
                std::cerr << "Could not open output file." << std::endl;
                return -1;
            }
        }

        if (avformat_write_header(m_format_Ctx.get(), nullptr) < 0)
        {
            std::cerr << "Could not write header." << std::endl;
            return -1;
        }
        m_header_written = true;
    }

    if (m_packet == nullptr)
    {
        AVPacket* tmp_packet = av_packet_alloc();
        m_packet = std::shared_ptr<AVPacket>(tmp_packet, [](AVPacket* p) { av_packet_free(&p); });
        av_new_packet(m_packet.get(), streamPacket.useful_pkt_size);
    }

    if (m_packet->size < streamPacket.useful_pkt_size)
    {
        // padding the small packet
        int grow_by = FFALIGN(streamPacket.useful_pkt_size, 64) + 1024;
        av_grow_packet(m_packet.get(), grow_by);
        m_packet->size = streamPacket.useful_pkt_size;
    }

    std::memcpy(m_packet->data, streamPacket.pkt_data.data(), streamPacket.useful_pkt_size);
    m_packet->stream_index = streamPacket.stream_index;

    if (true == m_mux_cfg.ts_recreate)
    {
        m_packet->pts = recreatePTS(streamPacket.stream_index);
        m_packet->dts = recreateDTS(streamPacket.stream_index);
    }
    else
    {
        m_packet->pts = streamPacket.pts;
        m_packet->dts = streamPacket.dts;
    }
    m_packet->pos = -1;

    if (av_interleaved_write_frame(m_format_Ctx.get(), m_packet.get()) < 0)
    {
        std::cerr << "Could not write frame." << std::endl;
        return -1;
    }

    m_stream_frames_count_map[streamPacket.stream_index]++;
    return 0;
}

std::shared_ptr<StreamReader> FFmpegMuxer::getStreamReader(const std::string& filename)
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Output format context is null." << std::endl;
        return nullptr;
    }

    std::shared_ptr<StreamReader> strReader = std::make_shared<FFmpegStreamReader>();
    int ret = strReader->openStreamReader(filename);
    if (ret < 0)
    {
        std::cerr << "Could not open stream reader." << std::endl;
        return nullptr;
    }

    return strReader;
}

int FFmpegMuxer::endStreamMux()
{
    if (m_format_Ctx == nullptr)
    {
        std::cerr << "Output format context is null." << std::endl;
        return -1;
    }

    av_write_trailer(m_format_Ctx.get());
    m_packet.reset();
    m_header_written = false;
}

} // namespace bsp_container