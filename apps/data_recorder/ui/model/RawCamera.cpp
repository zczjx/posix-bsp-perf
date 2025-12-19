#include "RawCamera.hpp"
#include <common/msg/CameraSensorMsg.hpp>
#include <iostream>
#include <thread>
#include <msgpack.hpp>

namespace apps
{
namespace data_recorder
{
namespace ui
{

RawCamera::RawCamera(const json& gui_ipc)
{
    setupCameraConsumer(gui_ipc);
}

RawCamera::~RawCamera()
{
    m_stopSignal.store(true);

    for (auto& thread: m_input_shmem_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
}

void RawCamera::setupCameraConsumer(const json& gui_ipc)
{
    for (const auto& sensor: gui_ipc["camera"])
    {
        std::string sensor_type = sensor["type"];
        const json& publisher = sensor["publisher"];
        m_input_shmem_ports[sensor["name"]] = std::make_pair(sensor_type, std::make_shared<SharedMemSubscriber>(publisher["topic"], publisher["shmem"], publisher["shmem_slots"], publisher["shmem_single_buffer_size"]));
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

void RawCamera::CameraConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port)
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

        if(shmem_msg.pixel_format.compare("RGB888") == 0 || shmem_msg.pixel_format.compare("RGBA8888") == 0)
        {
            emit rawCameraFrameUpdated(data_buffer.get(), shmem_msg.width, shmem_msg.height, QString::fromStdString(shmem_msg.pixel_format));
        }
        else
        {
            std::cerr << "GuiClient::CameraConsumerLoop() unsupported pixel format: " << shmem_msg.pixel_format << std::endl;
        }
    }
}

} // namespace ui
} // namespace data_recorder
} // namespace apps