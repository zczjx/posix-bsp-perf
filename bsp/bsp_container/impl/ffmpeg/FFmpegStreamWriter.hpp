#ifndef FFMPEG_STREAM_WRITER_HPP
#define FFMPEG_STREAM_WRITER_HPP

#include <bsp_container/StreamWriter.hpp>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
}

namespace bsp_container
{

class FFmpegStreamWriter : public StreamWriter
{
public:
    FFmpegStreamWriter() = default;
    FFmpegStreamWriter(const FFmpegStreamWriter &) = delete;
    FFmpegStreamWriter &operator=(const FFmpegStreamWriter &) = delete;
    ~FFmpegStreamWriter() override;

    /**
     * @brief Opens a stream writer for the given filename.
     * 
     * @param filename The name of the file to open for writing.
     * @param params A std::any object containing AVCodecParameters*.
     * @return int Returns 0 on success, or a negative error code on failure.
     */
    int openStreamWriter(const std::string &filename, std::any params) override;

    void closeStreamWriter() override;
    int writeHeader() override;
    int writePacket(const StreamPacket &packet) override;
    int writeTrailer(const uint8_t *data, size_t size) override;

private:
    std::shared_ptr<AVFormatContext> m_format_Ctx{nullptr};
};


} // namespace bsp_container
#endif // FFMPEG_STREAM_WRITER_HPP