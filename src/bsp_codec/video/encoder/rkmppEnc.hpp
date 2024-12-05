#ifndef __RKMPPENC_HPP__
#define __RKMPPENC_HPP__

#include <bsp_codec/IEncoder.hpp>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_frame.h>

namespace bsp_codec
{
class rkmppEnc : public IEncoder
{
public:
    rkmppEnc();

    ~rkmppEnc();

    int setup(EncodeConfig& cfg) override;

    void setEncodeReadyCallback(encodeReadyCallback callback, std::any userdata)
    {
        m_callback = callback;
        m_userdata = userdata;
    }

    int encode(EncodeFrame& frame_data) override;

    int getEncoderHeader(char* enc_buf, int max_size) override;

    int reset() override;

    size_t getFrameSize() override;

private:
    encodeReadyCallback m_callback{nullptr};
    std::any m_userdata;
};

} // namespace bsp_codec

#endif // __RKMPPENC_HPP__