#include "GuiClient.hpp"
#include <iostream>
#include <thread>

namespace apps
{
namespace data_recorder
{
namespace ui
{

GuiClient::GuiClient(int argc, char *argv[], const json& gui_ipc, const std::string& g2dPlatform):
    m_app(std::make_unique<QApplication>(argc, argv)),
    m_video_frame_widget(std::make_unique<VideoFrameWidget>())
{
    m_video_frame_widget->show();

    for (const auto& sensor: gui_ipc["camera"])
    {
        m_data_adapters[sensor["name"]] = std::make_shared<ImageFrameAdapter>(sensor, g2dPlatform);
    }

    for (const auto& data_adapter: m_data_adapters)
    {
        m_data_adapter_threads.push_back(std::thread([data_adapter]() {
            data_adapter.second->runLoop();
        }));
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
