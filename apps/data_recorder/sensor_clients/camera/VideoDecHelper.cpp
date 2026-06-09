#include "VideoDecHelper.hpp"
#include <bsp_image/ImageBuffer.hpp>
#include <bsp_g2d/BufferHelper.hpp>
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
    auto decoderCallback = [this](std::any userdata, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> frame)
    {
        std::lock_guard<std::mutex> lock(m_decoded_frame_queue_mutex);
        std::cout << "VideoDecHelper::decoderCallback() frame width: " << frame->view.desc.width << " frame height: " << frame->view.desc.height << std::endl;
        std::cout << "VideoDecHelper::decoderCallback() frame format: " << frame->view.desc.format << std::endl;
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

std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> VideoDecHelper::convertPixelFormat(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> frame)
{
    auto in_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, frame->view);

    bsp_perf::bsp_image::ImageDesc outDesc = frame->view.desc;
    outDesc.format = m_out_pixel_format;
    outDesc.widthStride = outDesc.width;
    outDesc.heightStride = outDesc.height;
    outDesc.dataSize = bsp_perf::bsp_image::imageDataSize(outDesc);
    auto out_frame = bsp_perf::bsp_image::makeHostImageBuffer(outDesc);

    auto out_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, out_frame->view);
    m_g2d->imageCvtColor(in_g2d_buf, out_g2d_buf, frame->view.desc.format, out_frame->view.desc.format);

    {
        bsp_g2d::BufferSyncGuard sync(
            m_g2d.get(),
            out_g2d_buf,
            IGraphics2D::SyncDirection::Bidirectional);
    }
    // 显式释放 G2D buffer 资源，防止 NvBufSurface 泄漏
    m_g2d->releaseBuffer(in_g2d_buf);
    m_g2d->releaseBuffer(out_g2d_buf);

    return out_frame;
}

std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> VideoDecHelper::getDecodedFrame()
{
    std::lock_guard<std::mutex> lock(m_decoded_frame_queue_mutex);

    if (m_decoded_frame_queue.empty())
    {
        return nullptr;
    }
    auto frame = m_decoded_frame_queue.front();
    m_decoded_frame_queue.pop();

    if (needPixelConverter(frame->view.desc.format))
    {
        return convertPixelFormat(frame);
    }

    return frame;
}

std::shared_ptr<VideoDecHelper::RtpBuffer> VideoDecHelper::createNewRtpBuffer(size_t min_size)
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

std::shared_ptr<VideoDecHelper::RtpBuffer> VideoDecHelper::getRtpVideoBuffer(size_t min_size)
{
    if (m_free_buffer_sort_queue.empty())
    {
        return createNewRtpBuffer(min_size);
    }
    {
        std::lock_guard<std::mutex> lock(m_free_buffer_sort_queue_mutex);
        auto buffer = m_free_buffer_sort_queue.back();
        if (buffer->buffer_size < min_size)
        {
            return createNewRtpBuffer(min_size);
        }
        buffer->valid_data_bytes = 0;
        buffer->payload_valid = false;
        m_free_buffer_sort_queue.pop_back();
        return buffer;
    }
}

int VideoDecHelper::releaseRtpVideoBuffer(std::shared_ptr<RtpBuffer> buffer)
{
    if (buffer == nullptr || buffer->buffer_size <= 0)
    {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(m_free_buffer_sort_queue_mutex);
        buffer->valid_data_bytes = 0;
        buffer->payload_valid = false;
        m_free_buffer_sort_queue.push_back(buffer);

        while (m_free_buffer_sort_queue.size() > m_free_buffer_sort_queue_size)
        {
            // erase operation will move all the elements after the erased element to the front
            // the back() will be invalid during erase operation if not guarded by a lock
            m_free_buffer_sort_queue.erase(m_free_buffer_sort_queue.begin());
        }

        std::sort(m_free_buffer_sort_queue.begin(), m_free_buffer_sort_queue.end(),
            [](const std::shared_ptr<RtpBuffer> a, const std::shared_ptr<RtpBuffer> b)
        {
            return a->buffer_size < b->buffer_size;
        });
    }

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
