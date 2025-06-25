#include "GuiClient.hpp"
#include <common/msg/CameraSensorMsg.hpp>
#include <iostream>
#include <thread>

namespace apps
{
namespace data_recorder
{
namespace ui
{
GuiClient::GuiClient(int argc, char *argv[], const json& gui_ipc):
    m_app(std::make_unique<QApplication>(argc, argv)),
    m_video_frame_widget(std::make_unique<VideoFrameWidget>())
{
    m_video_frame_widget->show();

    for (const auto& sensor: gui_ipc["camera"])
    {
        std::string sensor_type = sensor["type"];
        m_input_shmem_ports[sensor["name"]] = std::make_pair(sensor_type, std::make_shared<SharedMemSubscriber>(sensor["zmq_pub_info"], sensor["out_image_shm"], sensor["out_image_shm_slots"], sensor["out_image_shm_single_buffer_size"]));
    }

    for (const auto& input_pair: m_input_shmem_ports)
    {
        std::string sensor_type = input_pair.second.first;

        if (sensor_type.compare("camera") == 0)
        {
            m_input_shmem_threads.push_back(std::thread([this, input_pair]() {
                CameraConsumerLoop(input_pair.second.second);
            }));
        }
    }
}

void GuiClient::CameraConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port)
{

    std::shared_ptr<uint8_t[]> msg_buffer(new uint8_t[sizeof(CameraSensorMsg)]);
    std::shared_ptr<uint8_t[]> data_buffer{nullptr};

    while (!m_stopSignal.load())
    {
        size_t msg_size = input_shmem_port->receiveMsg(msg_buffer, sizeof(CameraSensorMsg));
        if (msg_size <= 0)
        {
            std::cerr << "GuiClient::CameraConsumerLoop() receive msg failed" << std::endl;
            continue;
        }

        msgpack::unpacked result = msgpack::unpack(reinterpret_cast<const char*>(msg_buffer.get()), msg_size);
        CameraSensorMsg shmem_msg = result.get().as<CameraSensorMsg>();

        if (data_buffer == nullptr)
        {
            data_buffer.reset(new uint8_t[shmem_msg.data_size]);
        }

        input_shmem_port->receiveSharedMemData(data_buffer, shmem_msg.data_size, shmem_msg.slot_index);

        if(shmem_msg.pixel_format.compare("RGB888") == 0)
        {
            m_video_frame_widget->setFrame(data_buffer.get(), shmem_msg.width, shmem_msg.height);
        }
        else
        {
            std::cerr << "GuiClient::CameraConsumerLoop() unsupported pixel format: " << shmem_msg.pixel_format << std::endl;
        }
    }
}

void GuiClient::runLoop()
{
    m_app->exec();
}

GuiClient::~GuiClient()
{
    m_stopSignal.store(true);

    for (auto& thread: m_input_shmem_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    m_app->quit();
}

} // namespace ui
} // namespace data_recorder
} // namespace apps
