#ifndef __RKMPPENC_HPP__
#define __RKMPPENC_HPP__

#include <bsp_codec/IEncoder.hpp>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_frame.h>

namespace bsp_codec
{

struct rkmppEncParams
{
    MppCodingType encoding_type{MPP_VIDEO_CodingAVC};
    RK_U32 width{0};
    RK_U32 height{0};
    RK_U32 hor_stride{0};
    RK_U32 ver_stride{0};
    MppFrameFormat frameFormat{MPP_FMT_YUV420P};

    RK_U32 split_mode{0};
    RK_U32 split_arg{0};
    RK_U32 split_out{0};

    // rate control runtime parameter
    RK_S32 fps_in_flex{0};
    RK_S32 fps_in_den{1};
    RK_S32 fps_in_num{30};
    RK_S32 fps_out_flex{0};
    RK_S32 fps_out_den{1};
    RK_S32 fps_out_num{30};
    RK_S32 bps{0};
    RK_S32 bps_max{0};
    RK_S32 bps_min{0};
    RK_S32 rc_mode{0};
    RK_S32 gop_mode{0};
    RK_S32 gop_len{0};

    /* general qp control */
    RK_S32 qp_init{0};
    RK_S32 qp_max{0};
    RK_S32 qp_max_i{0};
    RK_S32 qp_min{0};
    RK_S32 qp_min_i{0};

    RK_U32 constraint_set{0};
    RK_U32 rotation{0};
    RK_U32 mirroring{0};
    RK_U32 flip{0};

    MppEncHeaderMode header_mode{MPP_ENC_HEADER_MODE_DEFAULT};
};
class rkmppEnc : public IEncoder
{
public:
    rkmppEnc() = default;

    ~rkmppEnc();

    int setup(EncodeConfig& cfg) override;

    void setEncodeReadyCallback(encodeReadyCallback callback, std::any userdata) override
    {
        m_callback = callback;
        m_userdata = userdata;
    }

    int encode(EncodeInputBuffer& input_buf, EncodePacket& out_pkt) override;

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
    int calculateFrameSize(MppFrameFormat frameFormat);

private:
    encodeReadyCallback m_callback{nullptr};
    std::any m_userdata{nullptr};
    mpiContext m_ctx{};
    rkmppEncParams m_params{};
};

} // namespace bsp_codec

#endif // __RKMPPENC_HPP__