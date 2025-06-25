#include "VideoDecHelper.hpp"
#include <bsp_g2d/BytesPerPixel.hpp>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

namespace apps
{
namespace data_recorder
{

VideoDecHelper::VideoDecHelper(const std::string& decoder_name, const std::string& g2dPlatform, const std::string& out_pixel_format):
    m_decoder(bsp_codec::IDecoder::create(decoder_name)),
    m_g2d(IGraphics2D::create(g2dPlatform)),
    m_out_pixel_format(out_pixel_format)
{
}

int VideoDecHelper::setupAndStartDecoder(DecodeConfig& cfg)
{
    auto decoderCallback = [this](std::any userdata, std::shared_ptr<DecodeOutFrame> frame)
    {
        std::lock_guard<std::mutex> lock(m_decoded_frame_queue_mutex);
        std::cout << "VideoDecHelper::decoderCallback() frame->width: " << frame->width << " frame->height: " << frame->height << std::endl;
        std::cout << "VideoDecHelper::decoderCallback() frame->format: " << frame->format << std::endl;
        m_decoded_frame_queue.push(frame);

        while (m_decoded_frame_queue.size() > m_reserved_frame_num)
        {
            std::cerr << "VideoDecHelper::decoderCallback() drop the oldest frame" << std::endl;
            m_decoded_frame_queue.pop();
        }
    };

    m_decoder->setup(cfg);

    m_decoder->setDecodeReadyCallback(decoderCallback, nullptr);
    m_dec_thread = std::make_unique<std::thread>([this]() {decoderLoop();});
    return 0;
}

bool VideoDecHelper::needPixelConverter(const std::string& pixel_format)
{
    if (m_out_pixel_format.compare(pixel_format) == 0)
    {
        return false;
    }

    return true;
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
            rtp_pkt.reset();
        }

    }
}

int VideoDecHelper::sendToDecoder(std::shared_ptr<VideoDecHelper::RtpBuffer> rtp_pkt)
{
    std::lock_guard<std::mutex> lock(m_encode_pkt_queue_mutex);
    m_encode_pkt_queue.push(rtp_pkt);

    while (m_encode_pkt_queue.size() > m_reserved_encode_pkt_num)
    {
        std::cerr << "VideoDecHelper::sendToDecoder() drop the oldest rtp packet" << std::endl;
        auto oldest_pkt = m_encode_pkt_queue.front();
        releaseRtpVideoBuffer(oldest_pkt);
        m_encode_pkt_queue.pop();
        oldest_pkt.reset();
    }
    return 0;
}

std::shared_ptr<DecodeOutFrame> VideoDecHelper::convertPixelFormat(std::shared_ptr<DecodeOutFrame> frame)
{
    IGraphics2D::G2DBufferParams in_g2d_params = {
        .virtual_addr = frame->virt_addr,
        .rawBufferSize = frame->valid_data_size,
        .width = frame->width,
        .height = frame->height,
        .width_stride = frame->width_stride,
        .height_stride = frame->height_stride,
        .format = frame->format,
    };

    auto in_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", in_g2d_params);

    size_t out_data_size = frame->width * frame->height * bsp_g2d::BytesPerPixel::getInstance().getBytesPerPixel(m_out_pixel_format);

    std::shared_ptr<DecodeOutFrame> out_frame(new DecodeOutFrame(), [](DecodeOutFrame* frame) {
        if (frame->virt_addr != nullptr)
        {
            delete[] frame->virt_addr;
            frame->virt_addr = nullptr;
        }
        delete frame;
    });
    out_frame->width = frame->width;
    out_frame->height = frame->height;
    out_frame->format = m_out_pixel_format;
    out_frame->virt_addr = new uint8_t[out_data_size];
    out_frame->valid_data_size = out_data_size;
    out_frame->width_stride = frame->width;
    out_frame->height_stride = frame->height;


    IGraphics2D::G2DBufferParams out_g2d_params = {
    .virtual_addr = out_frame->virt_addr,
    .rawBufferSize = out_frame->valid_data_size,
    .width = out_frame->width,
    .height = out_frame->height,
    .width_stride = out_frame->width_stride,
    .height_stride = out_frame->height_stride,
    .format = m_out_pixel_format,
    };

    auto out_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", out_g2d_params);
    m_g2d->imageCvtColor(in_g2d_buf, out_g2d_buf, in_g2d_params.format, out_g2d_params.format);

    return out_frame;
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

    if (needPixelConverter(frame->format))
    {
        return convertPixelFormat(frame);
    }

    return frame;
}

std::shared_ptr<VideoDecHelper::RtpBuffer> VideoDecHelper::getRtpVideoBuffer(size_t min_size)
{
    if (m_free_buffer_sort_queue.empty() || m_free_buffer_sort_queue.back()->buffer_size < min_size)
    {
        std::shared_ptr<VideoDecHelper::RtpBuffer> buffer(new RtpBuffer(), [](RtpBuffer* buffer) {
            if (buffer->raw_data != nullptr)
            {
                buffer->raw_data.reset();
            }
            delete buffer;
        });
        size_t alloc_size = min_size * 2;
        buffer->raw_data = std::shared_ptr<uint8_t[]>(new uint8_t[alloc_size]);
        buffer->buffer_size = alloc_size;
        buffer->valid_data_bytes = 0;
        buffer->payload_valid = false;
        return buffer;
    }

    std::lock_guard<std::mutex> lock(m_free_buffer_sort_queue_mutex);
    assert(m_free_buffer_sort_queue.back() != nullptr);
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

    while (m_free_buffer_sort_queue.size() > m_free_buffer_sort_queue_size)
    {
        m_free_buffer_sort_queue.erase(m_free_buffer_sort_queue.begin());
    }

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
