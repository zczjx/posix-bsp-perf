#ifndef __FFMPEG_CODEC_HEADER_HPP__
#define __FFMPEG_CODEC_HEADER_HPP__
#include <unordered_map>
#include <string>
#include <mutex>
extern "C" {
#include <libavcodec/codec_id.h>
}

namespace bsp_container
{

class ffmpegCodecHeader
{
public:
    static ffmpegCodecHeader& getInstance()
    {
        static ffmpegCodecHeader instance;
        return instance;
    }

    enum AVCodecID strToFFmpegCoding(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return strToFFmpegCodingMap.at(str);
    }

    const std::string FFmpegCodingToStr(enum AVCodecID coding)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string str(avcodec_get_name(coding));
        return str;
    }

private:
    ffmpegCodecHeader() = default;
    ~ffmpegCodecHeader() = default;
    ffmpegCodecHeader(const ffmpegCodecHeader&) = delete;
    ffmpegCodecHeader& operator=(const ffmpegCodecHeader&) = delete;

private:
    std::mutex m_mutex;
    const std::unordered_map<std::string, enum AVCodecID> strToFFmpegCodingMap{
        /// video
        {"h264", AV_CODEC_ID_H264},
        {"hevc", AV_CODEC_ID_HEVC},
        {"265", AV_CODEC_ID_HEVC},
        {"vp9", AV_CODEC_ID_VP9},
        {"mpeg2", AV_CODEC_ID_MPEG2VIDEO},
        {"mpeg4", AV_CODEC_ID_MPEG4},
        /// audio
        {"aac", AV_CODEC_ID_AAC},
        {"mp3", AV_CODEC_ID_MP3},
    };
};

} // namespace bsp_container

#endif // __FFMPEG_CODEC_HEADER_HPP__