#ifndef __FFMPEG_STREAM_READER_HPP__
#define __FFMPEG_STREAM_READER_HPP__

#include <bsp_container/StreamReader.hpp>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
}

namespace bsp_container
{
class FFmpegStreamReader: public StreamReader
{
public:
    FFmpegStreamReader() = default;
    FFmpegStreamReader(const FFmpegStreamReader &) = delete;
    FFmpegStreamReader &operator=(const FFmpegStreamReader &) = delete;
    ~FFmpegStreamReader() override;

    int openStreamReader(const std::string& filename)  override;

    void closeStreamReader() override;

    std::any getStreamParams() override;

    int readPacket(StreamPacket& packet) override;


private:
    std::shared_ptr<AVFormatContext> m_format_Ctx{nullptr};
    std::shared_ptr<AVPacket> m_packet{nullptr};
};

}


#endif // __FFMPEG_STREAM_READER_HPP__