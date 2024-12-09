#ifndef __RKMPPENC_HPP__
#define __RKMPPENC_HPP__

#include <bsp_codec/IEncoder.hpp>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_frame.h>

namespace bsp_codec
{

struct rkmppEncParams
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
    int fps{30};
    MppCodingType encoding_type{MPP_VIDEO_CodingAVC};

    RK_U32 width{1920};
    RK_U32 height{1080};
    RK_U32 hor_stride{1920};
    RK_U32 ver_stride{1080};
    MppFrameFormat fmt{MPP_FMT_YUV420P};
    MppCodingType type{MPP_VIDEO_CodingAVC};

    RK_U32 osd_enable{0};
    RK_U32 osd_mode{0};
    RK_U32 split_mode{0};
    RK_U32 split_arg{0};
    RK_U32 split_out{0};

    RK_U32 user_data_enable{0};
    RK_U32 roi_enable{0};

    // rate control runtime parameter
    RK_S32 fps_in_flex{0};
    RK_S32 fps_in_den{1};
    RK_S32 fps_in_num{30};
    RK_S32 fps_out_flex{0};
    RK_S32 fps_out_den{1};
    RK_S32 fps_out_num{30};
    RK_S32 bps{2000000};
    RK_S32 bps_max{4000000};
    RK_S32 bps_min{1000000};
    RK_S32 rc_mode{0};
    RK_S32 gop_mode{0};
    RK_S32 gop_len{60};
    RK_S32 vi_len{0};

    /* general qp control */
    RK_S32 qp_init{26};
    RK_S32 qp_max{51};
    RK_S32 qp_max_i{51};
    RK_S32 qp_min{10};
    RK_S32 qp_min_i{10};
    RK_S32 qp_max_step{4}; /* delta qp between each two P frame */
    RK_S32 qp_delta_ip{2}; /* delta qp between I and P */
    RK_S32 qp_delta_vi{2}; /* delta qp between vi and P */

    RK_U32 constraint_set{0};
    RK_U32 rotation{0};
    RK_U32 mirroring{0};
    RK_U32 flip{0};

    MppEncHeaderMode header_mode{MPP_ENC_HEADER_MODE_DEFAULT};
    MppEncSeiMode sei_mode{MPP_ENC_SEI_MODE_ONE_FRAME};
};
class rkmppEnc : public IEncoder
{
public:
    rkmppEnc();

    ~rkmppEnc();

    int setup(EncodeConfig& cfg) override;

    void setEncodeReadyCallback(encodeReadyCallback callback, std::any userdata) override
    {
        m_callback = callback;
        m_userdata = userdata;
    }

    int encode(EncodeFrame& frame_data) override;

    int getEncoderHeader(std::string& headBuf) override;

    int reset() override;

    std::shared_ptr<EncodeInputBuffer> getInputBuffer() override;

    size_t getFrameSize() override
    {
        return m_ctx.frame_size;
    }

    struct mpiContext
    {
        MppCtx          mpp_ctx{nullptr};
        MppApi*         mpi{nullptr};
        RK_U32          eos{0};
        MppBufferGroup  frm_grp{nullptr};
        MppBufferGroup  buf_grp{nullptr};
        MppBuffer       pkt_buf{nullptr};
        MppBuffer       frm_buf {nullptr};
        MppBuffer       md_info{nullptr};
        size_t          mdinfo_size{0};
        size_t          frame_size{0};
    };

protected:
    int parseConfig(EncodeConfig& cfg);
    int setupMppEncCfg();

private:
    encodeReadyCallback m_callback{nullptr};
    std::any m_userdata;
    mpiContext m_ctx{};
    rkmppEncParams m_params{};
};

} // namespace bsp_codec

#endif // __RKMPPENC_HPP__