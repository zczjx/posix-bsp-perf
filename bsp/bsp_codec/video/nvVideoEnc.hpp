#ifndef __NVVIDEO_ENC_HPP__
#define __NVVIDEO_ENC_HPP__

#include "../IEncoder.hpp"
#include <NvVideoEncoder.h>
#include <NvBuffer.h>
#include <NvBufSurface.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <linux/videodev2.h>

namespace bsp_codec
{

// Internal structure to pass both v4l2_buffer and NvBuffer between functions
struct InternalBufferPair {
    std::shared_ptr<struct v4l2_buffer> v4l2_buf;
    NvBuffer* nvbuffer;
};

struct nvVideoEncParams
{
    uint32_t width{0};
    uint32_t height{0};
    uint32_t fps{30};
    uint32_t bitrate{4 * 1024 * 1024}; // 4 Mbps default
    uint32_t num_output_buffers{10};    // Number of buffers for the output plane (raw input)
    uint32_t num_capture_buffers{10};   // Number of buffers for the capture plane (encoded output)
    std::string encoding_type{"h264"};
    std::string frame_format{"YUV420SP"};
    uint32_t profile{0}; // 0 for default
    uint32_t level{(uint32_t)-1}; // -1 for auto
    enum v4l2_mpeg_video_bitrate_mode rate_control{V4L2_MPEG_VIDEO_BITRATE_MODE_CBR};
    uint32_t iframe_interval{30};
    uint32_t idr_interval{256};
};

class nvVideoEnc : public IEncoder
{
public:
    nvVideoEnc();
    ~nvVideoEnc();

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
    size_t getFrameSize() override;

private:
    void captureThreadFunc();
    int setupOutputPlane();
    int setupCapturePlane();
    uint32_t getV4L2PixelFormat(const std::string& format);

    nvVideoEncParams m_params{};
    NvVideoEncoder *m_encoder{nullptr};
    encodeReadyCallback m_callback{nullptr};
    std::any m_userdata;

    std::thread m_capture_thread;
    std::atomic<bool> m_stop_thread{false};
    std::atomic<bool> m_eos_sent{false};

    // For managing DMA-BUF file descriptors
    int m_output_plane_fds[32];
    int m_capture_plane_fds[32];

    size_t m_frame_size{0};
    std::string m_encoder_header; // Store SPS/PPS

    std::mutex m_encode_mutex;
    std::condition_variable m_encode_cv;
    bool m_ready_for_encode{false};

    // Track how many buffers have been queued (for initial fill phase)
    std::atomic<uint32_t> m_output_buffers_queued{0};
};

} // namespace bsp_codec

#endif // __NVVIDEO_ENC_HPP__

