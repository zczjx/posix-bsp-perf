#include "VideoDecHelper.hpp"
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>

namespace apps
{
namespace data_recorder
{

VideoDecHelper::VideoDecHelper(const std::string& decoder_name)
{
    if (decoder_name.compare("rkmpp") == 0)
    {
        m_decoder = bsp_codec::IDecoder::create("rkmpp");
    }
    else if (decoder_name.compare("ffmpeg") == 0)
    {
        m_decoder = bsp_codec::IDecoder::create("ffmpeg");
    }
    else
    {
        throw std::invalid_argument("Invalid decoder name specified.");
    }
}

int VideoDecHelper::setupAndStartDecoder(DecodeConfig& cfg)
{
    m_decoder->setup(cfg);
    m_decoder->setDecodeReadyCallback(decoderCallback, nullptr);
    m_dec_thread = std::make_unique<std::thread>([this]() {decoderLoop();});
    return 0;
}

void VideoDecHelper::decoderCallback(std::any userdata, std::shared_ptr<DecodeOutFrame> frame)
{
    auto video_dec_helper = std::any_cast<VideoDecHelper*>(userdata);
}

void VideoDecHelper::decoderLoop()
{
    while (true)
    {
        std::shared_ptr<RtpBuffer> rtp_pkt{nullptr};

        if (m_encode_pkt_queue.empty())
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_encode_pkt_queue_mutex);
            rtp_pkt = m_encode_pkt_queue.front();
            m_encode_pkt_queue.pop();
        }

        if (rtp_pkt != nullptr)
        {
            DecodePacket dec_pkt = {
                .data = rtp_pkt->payload.data,
                .pkt_size = rtp_pkt->payload.size,
                .pkt_eos = false,
            };
            m_decoder->decode(dec_pkt);
            releaseRtpVideoBuffer(rtp_pkt);
            rtp_pkt = nullptr;
        }

    }
}

int VideoDecHelper::sendToDecoder(std::shared_ptr<VideoDecHelper::RtpBuffer> rtp_pkt)
{
    std::lock_guard<std::mutex> lock(m_encode_pkt_queue_mutex);
    m_encode_pkt_queue.push(rtp_pkt);

    if (m_encode_pkt_queue.size() > m_reserved_encode_pkt_num)
    {
        std::cerr << "VideoDecHelper::sendToDecoder() drop the oldest rtp packet" << std::endl;
        auto oldest_pkt = m_encode_pkt_queue.front();
        m_encode_pkt_queue.pop();
        releaseRtpVideoBuffer(oldest_pkt);
    }
    return 0;
}

std::shared_ptr<DecodeOutFrame> VideoDecHelper::getDecodedFrame()
{
    std::lock_guard<std::mutex> lock(m_decoded_frame_queue_mutex);

    if (m_decoded_frame_queue.empty())
    {
        return nullptr;
    }
    auto frame = m_decoded_frame_queue.front();
    m_decoded_frame_queue.pop();
    return frame;
}

std::shared_ptr<VideoDecHelper::RtpBuffer> VideoDecHelper::getRtpVideoBuffer(size_t min_size)
{
    if (m_free_buffer_sort_queue.empty() || m_free_buffer_sort_queue.back()->buffer_size < min_size)
    {
        std::shared_ptr<VideoDecHelper::RtpBuffer> buffer = std::make_shared<VideoDecHelper::RtpBuffer>();
        size_t alloc_size = min_size * 2;
        buffer->raw_data = std::shared_ptr<uint8_t[]>(new uint8_t[alloc_size]);
        buffer->buffer_size = alloc_size;
        buffer->valid_data_bytes = 0;
        buffer->payload_valid = false;
        return buffer;
    }

    std::lock_guard<std::mutex> lock(m_free_buffer_sort_queue_mutex);
    auto buffer = m_free_buffer_sort_queue.back();
    buffer->valid_data_bytes = 0;
    buffer->payload_valid = false;
    m_free_buffer_sort_queue.pop_back();
    return buffer;
}

int VideoDecHelper::releaseRtpVideoBuffer(std::shared_ptr<RtpBuffer> buffer)
{
    if (buffer->buffer_size <= 0)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(m_free_buffer_sort_queue_mutex);
    buffer->valid_data_bytes = 0;
    buffer->payload_valid = false;
    m_free_buffer_sort_queue.push_back(buffer);
    std::sort(m_free_buffer_sort_queue.begin(), m_free_buffer_sort_queue.end(),
        [](const std::shared_ptr<RtpBuffer> a, const std::shared_ptr<RtpBuffer> b)
    {
        return a->buffer_size < b->buffer_size;
    });

    return 0;
}

VideoDecHelper::~VideoDecHelper()
{
    if (m_dec_thread->joinable())
    {
        m_stopSignal.store(true);
        m_dec_thread->join();
    }
    m_decoder.reset();
}




} // namespace data_recorder
} // namespace apps
