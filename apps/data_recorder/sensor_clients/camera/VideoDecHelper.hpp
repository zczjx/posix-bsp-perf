#ifndef __VIDEO_DEC_HELPER_HPP__
#define __VIDEO_DEC_HELPER_HPP__

#include <bsp_codec/IDecoder.hpp>
#include <protocol/RtpHeader.hpp>

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
using namespace bsp_codec;
using namespace bsp_perf::protocol;

namespace apps
{
namespace data_recorder
{

class VideoDecHelper
{
public:

    struct RtpBuffer
    {
        std::shared_ptr<uint8_t[]> raw_data;
        size_t buffer_size{0};
        size_t valid_data_bytes{0};
        RtpPayload payload;
        bool payload_valid{false};
    };

    VideoDecHelper(const std::string& decoder_name);

    int setupAndStartDecoder(DecodeConfig& cfg);

    std::shared_ptr<RtpBuffer> getRtpVideoBuffer(size_t min_size);

    int releaseRtpVideoBuffer(std::shared_ptr<RtpBuffer> buffer);

    int sendToDecoder(std::shared_ptr<VideoDecHelper::RtpBuffer> rtp_pkt);

    std::shared_ptr<DecodeOutFrame> getDecodedFrame();

    ~VideoDecHelper();

private:
    void decoderLoop();

private:
    std::unique_ptr<bsp_codec::IDecoder> m_decoder;
    std::unique_ptr<std::thread> m_dec_thread;
    std::atomic<bool> m_stopSignal{false};

    // largest raw_data_bytes at the end
    std::vector<std::shared_ptr<RtpBuffer>> m_free_buffer_sort_queue;
    std::mutex m_free_buffer_sort_queue_mutex;
    const size_t m_free_buffer_sort_queue_size{30};

    std::queue<std::shared_ptr<DecodeOutFrame>> m_decoded_frame_queue;
    std::mutex m_decoded_frame_queue_mutex;
    const size_t m_reserved_frame_num{30};

    std::queue<std::shared_ptr<RtpBuffer>> m_encode_pkt_queue;
    std::mutex m_encode_pkt_queue_mutex;
    const size_t m_reserved_encode_pkt_num{30};
};

} // namespace data_recorder
} // namespace apps

#endif // __VIDEO_DEC_HELPER_HPP__
