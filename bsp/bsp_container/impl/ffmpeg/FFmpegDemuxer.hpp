#ifndef __FFMPEG_DEMUXER_HPP__
#define __FFMPEG_DEMUXER_HPP__

#include <bsp_container/IDemuxer.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace bsp_container
{
class FFmpegDemuxer : public IDemuxer
{
public:
    FFmpegDemuxer() = default;
    ~FFmpegDemuxer() override;

    int openContainerDemux(const std::string& path) override;

    void closeContainerDemux() override;

    int getContainerInfo(ContainerInfo& containerInfo) override;

    int readStreamPacket(StreamPacket& streamPacket) override;

    int seekStreamFrame(int stream_index, int64_t timestamp) override;

    std::shared_ptr<StreamWriter> getStreamWriter(int stream_index, const std::string& filename) override;

private:
    void getStreamDisposition(const AVStream* stream, std::vector<std::string>& disposition_list);

private:
    std::shared_ptr<AVFormatContext> m_format_Ctx{nullptr};
    std::shared_ptr<AVPacket> m_packet{nullptr};
};

} // namespace bsp_container

#endif // __FFMPEG_DEMUXER_HPP__
