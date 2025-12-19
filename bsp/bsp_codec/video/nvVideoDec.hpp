#ifndef __NVDEC_DEC_HPP__
#define __NVDEC_DEC_HPP__

#include <bsp_codec/IDecoder.hpp>
#include <NvVideoDecoder.h>
#include <NvBufSurface.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace bsp_codec
{

struct nvVideoDecParams
{
    int fps{30};
    uint32_t decoder_pixfmt{0};  // V4L2 pixel format
    bool input_nalu{true};
    int num_output_buffers{10};
    int num_capture_buffers{6};
    int extra_cap_plane_buffer{1};
};

class nvVideoDec : public IDecoder
{
public:
    nvVideoDec() = default;
    ~nvVideoDec();

    // Implement necessary methods from IDecoder
    int setup(DecodeConfig& cfg) override;
    void setDecodeReadyCallback(decodeReadyCallback callback, std::any userdata) override
    {
        m_callback = callback;
        m_userdata = userdata;
    }
    int decode(DecodePacket& pkt_data) override;
    int tearDown() override;

private:
    // Internal methods
    int startDecoding();
    int setupDecoder();
    int queryAndSetCapture();
    void captureThreadFunc();
    void decoderThreadFunc();
    uint32_t getV4L2PixelFormat(const std::string& encoding);

    // Decoder state
    NvVideoDecoder* m_dec{nullptr};
    nvVideoDecParams m_params{};
    decodeReadyCallback m_callback{nullptr};
    std::any m_userdata;

    // Threading
    std::unique_ptr<std::thread> m_capture_thread;
    std::unique_ptr<std::thread> m_decoder_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_got_error{false};
    std::atomic<bool> m_got_eos{false};
    std::atomic<bool> m_capture_plane_started{false};

    // Buffer management
    int m_dmabuf_fds[32]{0};
    int m_num_capture_buffers{0};
    int m_dst_dma_fd{-1};

    // Resolution
    uint32_t m_display_width{0};
    uint32_t m_display_height{0};
    uint32_t m_video_width{0};
    uint32_t m_video_height{0};

    // Packet queue for decoding
    struct PacketInfo {
        std::vector<uint8_t> data;
        bool is_eos{false};
    };
    std::queue<PacketInfo> m_packet_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;

    // Synchronization
    std::mutex m_setup_mutex;
    std::condition_variable m_setup_cv;

};

} // namespace bsp_codec

#endif // __NVDEC_DEC_HPP__

