#include "CameraClient.hpp"
#include <iostream>
#include <chrono>
#include <stdexcept>

namespace apps
{
namespace data_recorder
{

CameraClient::CameraClient(const json& sensor_context, const json& vehicle_info, const json& node_ipc)
    : SensorClient(sensor_context)
{
    setupInputConfig(sensor_context, vehicle_info);
    setupOutputConfig(node_ipc);

    if (m_target_platform.compare("rk3588") == 0)
    {
        m_video_dec_helper = std::make_unique<VideoDecHelper>("rkmpp", "rkrga", m_out_pixel_format);
    }
    else
    {
        throw std::runtime_error("Unsupported platform: " + m_target_platform);
    }
    DecodeConfig cfg = {
        .encoding = "h264",
        .fps = 75,
    };
    m_video_dec_helper->setupAndStartDecoder(cfg);
    m_main_thread = std::make_unique<std::thread>([this]() {runLoop();});
    m_consumer_thread = std::make_unique<std::thread>([this]() {consumerLoop();});
}

void CameraClient::setupInputConfig(const json& sensor_context, const json& vehicle_info)
{
    m_xres = sensor_context["image_size_x"];
    m_yres = sensor_context["image_size_y"];
    m_raw_pixel_format = sensor_context["raw_pixel_format"];
    m_target_platform = vehicle_info["target_platform"];
    m_input_port = std::make_shared<ZmqSubscriber>(sensor_context["zmq_transport"]);
}

void CameraClient::setupOutputConfig(const json& node_ipc)
{
    m_out_pixel_format = node_ipc["out_image_pixel_format"];
    m_output_shmem_port = std::make_shared<SharedMemPublisher>(node_ipc["zmq_pub_info"],
                            node_ipc["out_image_shm"], node_ipc["out_image_shm_slots"],
                            node_ipc["out_image_shm_single_buffer_size"]);
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

void CameraClient::fillCameraSensorMsg(CameraSensorMsg& msg, size_t data_size, size_t slot_index)
{
    msg.publisher_id = getSensorName();
    msg.pixel_format = m_out_pixel_format;
    msg.width = m_xres;
    msg.height = m_yres;
    msg.data_size = data_size;
    msg.slot_index = slot_index;
}

std::shared_ptr<DecodeOutFrame> CameraClient::getCameraVideoFrame()
{
    return m_video_dec_helper->getDecodedFrame();
}

void CameraClient::consumerLoop()
{
    size_t frame_count = 0;
    auto last_time = std::chrono::steady_clock::now();
    CameraSensorMsg sensor_msg;
    msgpack::sbuffer msg_buffer;
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
            fillCameraSensorMsg(sensor_msg, frame->valid_data_size, m_output_shmem_port->getFreeSlotIndex());
            msg_buffer.clear();
            msgpack::pack(msg_buffer, sensor_msg);
            m_output_shmem_port->publishData(reinterpret_cast<const uint8_t*>(msg_buffer.data()),
                                            msg_buffer.size(), frame->virt_addr,
                                            sensor_msg.slot_index, sensor_msg.data_size);
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
        size_t valid_bytes = m_input_port->receiveDataMore(rtp_buffer->raw_data, rtp_buffer->buffer_size);
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
