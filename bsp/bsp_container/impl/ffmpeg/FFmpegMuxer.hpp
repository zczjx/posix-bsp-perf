#ifndef __FFMPEG_MUXER_HPP__
#define __FFMPEG_MUXER_HPP__

#include <bsp_container/IMuxer.hpp>
#include <memory>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace bsp_container
{
class FFmpegMuxer : public IMuxer
{
public:
    FFmpegMuxer() = default;
    ~FFmpegMuxer() override;

    int openContainerMux(const std::string& path, MuxConfig& config) override;

    void closeContainerMux() override;

    int addStream(StreamInfo& streamInfo) override;

    int addStream(std::shared_ptr<StreamReader> strReader) override;

    int writeStreamPacket(StreamPacket& streamPacket) override;

    std::shared_ptr<StreamReader> getStreamReader(const std::string& filename) override;

    int endStreamMux() override;

private:
    void setupVideoStreamParams(AVStream* out_stream, StreamInfo& streamInfo);

    void setupAudioStreamParams(AVStream* out_stream, StreamInfo& streamInfo);

    int64_t recreatePTS(int stream_index);

    int64_t recreateDTS(int stream_index);

private:
    std::shared_ptr<AVFormatContext> m_format_Ctx{nullptr};
    bool m_header_written{false};
    std::string m_path{};
    std::shared_ptr<AVPacket> m_packet{nullptr};
    MuxConfig m_mux_cfg{};
    std::unordered_map<int, int> m_stream_frames_count_map{};
};

} // namespace bsp_container

#endif // __FFMPEG_MUXER_HPP__