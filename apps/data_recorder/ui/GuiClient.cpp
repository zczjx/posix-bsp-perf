#include "GuiClient.hpp"
#include <iostream>
#include <thread>

namespace apps
{
namespace data_recorder
{
namespace ui
{

GuiClient::GuiClient(int argc, char *argv[], const json& gui_ipc):
    m_app(std::make_unique<QApplication>(argc, argv))
{
    m_video_frame_widget->show();

    for (const auto& sensor: gui_ipc["camera"])
    {
        m_input_shmem_ports[sensor["name"]] = std::make_shared<SharedMemSubscriber>(sensor["zmq_pub_info"], sensor["out_image_shm"], sensor["out_image_shm_slots"], sensor["out_image_shm_single_buffer_size"]);
    }

    for (const auto& input_port_pair: m_input_shmem_ports)
    {
        m_input_shmem_threads.push_back(std::thread([this, input_port_pair]() {
            dataConsumerLoop(input_port_pair.second);
        }));
    }
}

void GuiClient::dataConsumerLoop(std::shared_ptr<SharedMemSubscriber> input_shmem_port)
{
    while (true)
    {
        input_shmem_port->receiveSharedMemData(input_shmem_port->getSharedMemData(), input_shmem_port->getSharedMemDataSize(), input_shmem_port->getSharedMemDataSlotIndex());
    }
}

void GuiClient::installGuiUpdateCallback()
{
    for (const auto& data_adapter: m_data_adapters)
    {
        if (data_adapter.second->getType() == "camera")
        {
            installFlipFrameCallbackCallback(data_adapter.second);
        }
    }
}

void GuiClient::installFlipFrameCallbackCallback(std::shared_ptr<ImageFrameAdapter> data_adapter)
{
    data_adapter->setFlipFrameCallback([this](const uint8_t* data, int width, int height) {
        m_video_frame_widget->setFrame(data, width, height);
    });
}

void GuiClient::runLoop()
{
    installGuiUpdateCallback();

    m_app->exec();
}

GuiClient::~GuiClient()
{
    for (auto& data_adapter: m_data_adapters)
    {
        data_adapter.second->stop();
    }

    for (auto& thread: m_data_adapter_threads)
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
