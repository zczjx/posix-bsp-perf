#ifndef __FFMPEG_DEMUXER_HPP__
#define __FFMPEG_DEMUXER_HPP__

#include <bsp_container/IDemuxer.hpp>
#include <libavformat/avformat.h>

namespace bsp_container
{
class FFmpegDemuxer : public IDemuxer
{
public:
    FFmpegDemuxer() = default;
    ~FFmpegDemuxer() override;

    int openContainerParser(const std::string& path) override;

    void closeContainerParser() override;

    int getContainerInfo(ContainerInfo& containerInfo) override;

    int readStreamPacket(StreamPacket& streamPacket) override;


private:

    std::shared_ptr<AVFormatContext> m_format_Ctx{nullptr};
    std::shared_ptr<AVPacket> m_packet{nullptr};
};

} // namespace bsp_container

#endif // __FFMPEG_DEMUXER_HPP__
