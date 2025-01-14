#ifndef __FFMPEG_MUXER_HPP__
#define __FFMPEG_MUXER_HPP__

#include <bsp_container/IMuxer.hpp>
#include <libavformat/avformat.h>
#include <memory>
#include <string>

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

    int writeStreamPacket(StreamPacket& streamPacket) override;

    int endStreamMux() override;

private:
    void setupVideoStreamParams(AVStream* out_stream, StreamInfo& streamInfo);
    void setupAudioStreamParams(AVStream* out_stream, StreamInfo& streamInfo);

private:
    std::shared_ptr<AVFormatContext> m_format_Ctx{nullptr};
    bool m_header_written{false};
    std::string m_path{};
};

} // namespace bsp_container

#endif // __FFMPEG_MUXER_HPP__