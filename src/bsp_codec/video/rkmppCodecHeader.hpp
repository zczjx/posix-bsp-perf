#ifndef __RKMPP_CODEC_HEADER_HPP__
#define __RKMPP_CODEC_HEADER_HPP__
#include <rockchip/mpp_frame.h>
#include <rockchip/rk_type.h>
#include <unordered_map>
#include <string>
#include <mutex>

namespace bsp_codec
{

class rkmppCodecHeader
{
public:
    static rkmppCodecHeader& getInstance()
    {
        static rkmppCodecHeader instance;
        return instance;
    }

    MppCodingType strToMppCoding(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return strToMppCodingMap.at(str);
    }

    const std::string& mppCodingToStr(MppCodingType coding)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return mppCodingToStrMap.at(coding);
    }

    MppFrameFormat strToMppFrameFormat(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return strToMppFrameFormatMap.at(str);
    }

    const std::string& mppFrameFormatToStr(MppFrameFormat frameFormat)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return mppFrameFormatToStrMap.at(frameFormat);
    }

private:
    rkmppCodecHeader() = default;
    ~rkmppCodecHeader() = default;
    rkmppCodecHeader(const rkmppCodecHeader&) = delete;
    rkmppCodecHeader& operator=(const rkmppCodecHeader&) = delete;

private:
    std::mutex m_mutex;
    const std::unordered_map<std::string, MppCodingType> strToMppCodingMap{
        {"auto_detect", MPP_VIDEO_CodingAutoDetect},
        {"mpeg2", MPP_VIDEO_CodingMPEG2},
        {"h263", MPP_VIDEO_CodingH263},
        {"mpeg4", MPP_VIDEO_CodingMPEG4},
        {"wmv", MPP_VIDEO_CodingWMV},
        {"rv", MPP_VIDEO_CodingRV},
        {"h264", MPP_VIDEO_CodingAVC},
        {"mjpeg", MPP_VIDEO_CodingMJPEG},
        {"vp8", MPP_VIDEO_CodingVP8},
        {"vp9", MPP_VIDEO_CodingVP9},
        {"vc1", MPP_VIDEO_CodingVC1},
        {"flv1", MPP_VIDEO_CodingFLV1},
        {"divx3", MPP_VIDEO_CodingDIVX3},
        {"vp6", MPP_VIDEO_CodingVP6},
        {"h265", MPP_VIDEO_CodingHEVC},
        {"avsplus", MPP_VIDEO_CodingAVSPLUS},
        {"avs", MPP_VIDEO_CodingAVS},
        {"avs2", MPP_VIDEO_CodingAVS2},
        {"av1", MPP_VIDEO_CodingAV1}
    };
    const std::unordered_map<MppCodingType, std::string> mppCodingToStrMap{
        {MPP_VIDEO_CodingAutoDetect, "auto_detect"},
        {MPP_VIDEO_CodingMPEG2, "mpeg2"},
        {MPP_VIDEO_CodingH263, "h263"},
        {MPP_VIDEO_CodingMPEG4, "mpeg4"},
        {MPP_VIDEO_CodingWMV, "wmv"},
        {MPP_VIDEO_CodingRV, "rv"},
        {MPP_VIDEO_CodingAVC, "h264"},
        {MPP_VIDEO_CodingMJPEG, "mjpeg"},
        {MPP_VIDEO_CodingVP8, "vp8"},
        {MPP_VIDEO_CodingVP9, "vp9"},
        {MPP_VIDEO_CodingVC1, "vc1"},
        {MPP_VIDEO_CodingFLV1, "flv1"},
        {MPP_VIDEO_CodingDIVX3, "divx3"},
        {MPP_VIDEO_CodingVP6, "vp6"},
        {MPP_VIDEO_CodingHEVC, "h265"},
        {MPP_VIDEO_CodingAVSPLUS, "avsplus"},
        {MPP_VIDEO_CodingAVS, "avs"},
        {MPP_VIDEO_CodingAVS2, "avs2"},
        {MPP_VIDEO_CodingAV1, "av1"}
    };
    const std::unordered_map<std::string, MppFrameFormat> strToMppFrameFormatMap{
        {"YUV420SP", MPP_FMT_YUV420SP},
        {"YUV420SP_10BIT", MPP_FMT_YUV420SP_10BIT},
        {"YUV422SP", MPP_FMT_YUV422SP},
        {"YUV422SP_10BIT", MPP_FMT_YUV422SP_10BIT},
        {"YUV420P", MPP_FMT_YUV420P},
        {"YUV420SP_VU", MPP_FMT_YUV420SP_VU},
        {"YUV422P", MPP_FMT_YUV422P},
        {"YUV422SP_VU", MPP_FMT_YUV422SP_VU},
        {"YUV422_YUYV", MPP_FMT_YUV422_YUYV},
        {"YUV422_YVYU", MPP_FMT_YUV422_YVYU},
        {"YUV422_UYVY", MPP_FMT_YUV422_UYVY},
        {"YUV422_VYUY", MPP_FMT_YUV422_VYUY},
        {"YUV400", MPP_FMT_YUV400},
        {"YUV440SP", MPP_FMT_YUV440SP},
        {"YUV411SP", MPP_FMT_YUV411SP},
        {"YUV444SP", MPP_FMT_YUV444SP},
        {"YUV444P", MPP_FMT_YUV444P},
        {"RGB565", MPP_FMT_RGB565},
        {"BGR565", MPP_FMT_BGR565},
        {"RGB555", MPP_FMT_RGB555},
        {"BGR555", MPP_FMT_BGR555},
        {"RGB444", MPP_FMT_RGB444},
        {"BGR444", MPP_FMT_BGR444},
        {"RGB888", MPP_FMT_RGB888},
        {"BGR888", MPP_FMT_BGR888},
        {"RGB101010", MPP_FMT_RGB101010},
        {"BGR101010", MPP_FMT_BGR101010},
        {"ARGB8888", MPP_FMT_ARGB8888},
        {"ABGR8888", MPP_FMT_ABGR8888},
        {"BGRA8888", MPP_FMT_BGRA8888},
        {"RGBA8888", MPP_FMT_RGBA8888}
    };
    const std::unordered_map<MppFrameFormat, std::string> mppFrameFormatToStrMap{
        {MPP_FMT_YUV420SP, "YUV420SP"},
        {MPP_FMT_YUV420SP_10BIT, "YUV420SP_10BIT"},
        {MPP_FMT_YUV422SP, "YUV422SP"},
        {MPP_FMT_YUV422SP_10BIT, "YUV422SP_10BIT"},
        {MPP_FMT_YUV420P, "YUV420P"},
        {MPP_FMT_YUV420SP_VU, "YUV420SP_VU"},
        {MPP_FMT_YUV422P, "YUV422P"},
        {MPP_FMT_YUV422SP_VU, "YUV422SP_VU"},
        {MPP_FMT_YUV422_YUYV, "YUV422_YUYV"},
        {MPP_FMT_YUV422_YVYU, "YUV422_YVYU"},
        {MPP_FMT_YUV422_UYVY, "YUV422_UYVY"},
        {MPP_FMT_YUV422_VYUY, "YUV422_VYUY"},
        {MPP_FMT_YUV400, "YUV400"},
        {MPP_FMT_YUV440SP, "YUV440SP"},
        {MPP_FMT_YUV411SP, "YUV411SP"},
        {MPP_FMT_YUV444SP, "YUV444SP"},
        {MPP_FMT_YUV444P, "YUV444P"},
        {MPP_FMT_RGB565, "RGB565"},
        {MPP_FMT_BGR565, "BGR565"},
        {MPP_FMT_RGB555, "RGB555"},
        {MPP_FMT_BGR555, "BGR555"},
        {MPP_FMT_RGB444, "RGB444"},
        {MPP_FMT_BGR444, "BGR444"},
        {MPP_FMT_RGB888, "RGB888"},
        {MPP_FMT_BGR888, "BGR888"},
        {MPP_FMT_RGB101010, "RGB101010"},
        {MPP_FMT_BGR101010, "BGR101010"},
        {MPP_FMT_ARGB8888, "ARGB8888"},
        {MPP_FMT_ABGR8888, "ABGR8888"},
        {MPP_FMT_BGRA8888, "BGRA8888"},
        {MPP_FMT_RGBA8888, "RGBA8888"}
    };

};

} // namespace bsp_codec

#endif // __RKMPP_CODEC_HEADER_HPP__