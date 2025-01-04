#ifndef RKMPPDEC_HPP
#define RKMPPDEC_HPP

#include <bsp_codec/IDecoder.hpp>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_frame.h>

namespace bsp_codec
{

struct rkmppDecParams
{
    int fps{0};
    unsigned long last_frame_time_ms{0};
    MppCodingType encoding_type{MPP_VIDEO_CodingUnused};
    RK_U32 need_split{1};
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