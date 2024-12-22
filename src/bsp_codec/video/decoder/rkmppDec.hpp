#ifndef RKMPPDEC_HPP
#define RKMPPDEC_HPP

#include <bsp_codec/IDecoder.hpp>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_frame.h>

namespace bsp_codec
{

struct rkmppDecParams
{
    const std::unordered_map<std::string, MppCodingType> m_encoding_map{
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
    int fps{0};
    unsigned long last_frame_time_ms{0};
    MppCodingType encoding_type{MPP_VIDEO_CodingUnused};
    RK_U32 need_split{1};

    const std::unordered_map<MppFrameFormat, std::string> frame_format_map = {
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

class rkmppDec : public IDecoder
{
public:
    rkmppDec() = default;
    ~rkmppDec();

    // Implement necessary methods from IDecoder
    int setup(DecodeConfig& cfg) override;
    void setDecodeReadyCallback(decodeReadyCallback callback, std::any userdata) override
    {
        m_callback = callback;
        m_userdata = userdata;
    }
    int decode(DecodePacket& pkt_data) override;
    int reset() override;
    struct mpiContext
    {
        MppCtx          mpp_ctx{nullptr};
        MppApi*         mpi{nullptr};
        RK_U32          eos{0};
        char            *buf{nullptr};

        MppBufferGroup  frm_grp{nullptr};
        MppBufferGroup  pkt_grp{nullptr};
        MppPacket       packet{nullptr};
        size_t          packet_size{2400 * 1300 * 3 / 2};
        MppFrame        frame{nullptr};

        RK_S32          frame_count{0};
        RK_S32          frame_num{0};
        size_t          max_usage{0};
    };

private:

    rkmppDecParams m_params{};
    mpiContext m_ctx{};
    decodeReadyCallback m_callback{nullptr};
    std::any m_userdata;
    MppPacket m_packet{nullptr};
    MppFrame  m_frame{nullptr};
};

} // namespace bsp_codec

#endif // RKMPPDEC_HPP