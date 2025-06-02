#include "CameraClient.hpp"
#include <iostream>
#include <chrono>

namespace apps
{
namespace data_recorder
{

CameraClient::CameraClient(const json& sensor_context, const json& vehicle_info, const json& sensor_ipc)
    : SensorClient(sensor_context, sensor_ipc)
{
    m_xres = sensor_context["image_size_x"];
    m_yres = sensor_context["image_size_y"];
    m_raw_pixel_format = sensor_context["raw_pixel_format"];
    m_target_platform = vehicle_info["target_platform"];
    if (m_target_platform.compare("rk3588") == 0)
    {
        m_video_dec_helper = std::make_unique<VideoDecHelper>("rkmpp");
    }
    else
    {
        m_video_dec_helper = std::make_unique<VideoDecHelper>("ffmpeg");
    }
    DecodeConfig cfg = {
        .encoding = "h264",
        .fps = 65,
    };
    m_video_dec_helper->setupAndStartDecoder(cfg);
    m_main_thread = std::make_unique<std::thread>([this]() {runLoop();});
    m_consumer_thread = std::make_unique<std::thread>([this]() {consumerLoop();});
}

CameraClient::~CameraClient()
{
    if (m_main_thread->joinable() || m_consumer_thread->joinable())
    {
        m_stopSignal.store(true);
        m_consumer_thread->join();
        m_main_thread->join();
    }
}

std::shared_ptr<DecodeOutFrame> CameraClient::getCameraVideoFrame()
{
    return m_video_dec_helper->getDecodedFrame();
}

void CameraClient::consumerLoop()
{
    size_t frame_count = 0;
    auto last_time = std::chrono::steady_clock::now();
    while (!m_stopSignal.load())
    {
        std::shared_ptr<DecodeOutFrame> frame = getCameraVideoFrame();
        if (frame != nullptr)
        {
            frame_count++;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_time).count();
            if (elapsed >= 1)
            {
                std::cout << "CameraClient::Decoding FPS: " << frame_count << std::endl;
                frame_count = 0;
                last_time = now;
            }
            pubSensorData(frame->virt_addr, frame->valid_data_size);
            frame.reset();
        }
    }
    std::cout << "CameraClient::consumerLoop() end, frame_count: " << frame_count << std::endl;
}

void CameraClient::runLoop()
{
    size_t frame_count = 0;
    std::cout << "runLoop" << std::endl;
    size_t min_buffer_size = 1024 * 1024;
    RtpHeader header;
    auto last_time = std::chrono::steady_clock::now();
    while(!m_stopSignal.load())
    {
        std::shared_ptr<VideoDecHelper::RtpBuffer> rtp_buffer = m_video_dec_helper->getRtpVideoBuffer(min_buffer_size) ;

        size_t valid_bytes = recvIpcDataMore(rtp_buffer->raw_data, rtp_buffer->buffer_size);
        if (valid_bytes <= 0)
        {
            std::cerr << "recvIpcDataMore failed, buffer size may be too small" << std::endl;
            min_buffer_size *= 2;
            continue;
        }

        rtp_buffer->valid_data_bytes = valid_bytes;
        int ret = m_rtp_reader.parseHeader(rtp_buffer->raw_data.get(), valid_bytes, header);
        if (ret < 0)
        {
            std::cerr << "parseHeader failed" << std::endl;
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(m_rtp_info_mutex);
            m_rtp_info = header;
        }

        rtp_buffer->payload = m_rtp_reader.extractPayload(rtp_buffer->raw_data.get(), valid_bytes);
        if (rtp_buffer->payload.size <= 0)
        {
            std::cerr << "extractPayload failed" << std::endl;
            continue;
        }
        rtp_buffer->payload_valid = true;
        m_video_dec_helper->sendToDecoder(rtp_buffer);
        rtp_buffer.reset();

        frame_count++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_time).count();

        if (elapsed >= 1)
        {
            std::cout << "camera client frame_count: " << frame_count << std::endl;
            std::cout << "camera client recv FPS: " << frame_count << std::endl;
            frame_count = 0;
            last_time = now;
        }
    }
    std::cout << "runLoop end" << std::endl;
}


} // namespace data_recorder
} // namespace apps
