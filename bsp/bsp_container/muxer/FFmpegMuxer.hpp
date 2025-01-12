#ifndef __FFMPEG_MUXER_HPP__
#define __FFMPEG_MUXER_HPP__

#include "IMuxer.hpp"
#include <libavformat/avformat.h>

namespace bsp_container
{
class FFmpegMuxer : public IMuxer
{
public:
    FFmpegMuxer() = default;
    ~FFmpegMuxer() override;

    int openContainerMux(const std::string& path) override;

    void closeContainerMux() override;

    int addStream(StreamInfo& streamInfo) override;

    int writeStreamPacket(StreamPacket& streamPacket);

private:
    std::shared_ptr<AVFormatContext> m_format_Ctx{nullptr};
    std::shared_ptr<AVPacket> m_packet{nullptr};
    bool m_header_written{false};
};

} // namespace bsp_container

#endif // __FFMPEG_MUXER_HPP__