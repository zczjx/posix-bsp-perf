
#include "GuiClient.hpp"
#include <iostream>
#include <thread>

namespace apps
{
namespace data_recorder
{

GuiClient::GuiClient(int argc, char *argv[], const json& gui_ipc, const std::string& g2dPlatform):
    m_app(std::make_unique<QApplication>(argc, argv)),
    m_video_frame_widget(std::make_unique<VideoFrameWidget>())
{
    m_video_frame_widget->show();

    for (const auto& sensor: gui_ipc["camera"])
    {
        m_data_sources[sensor["name"]] = std::make_shared<DataSource>(sensor, g2dPlatform);
    }

    for (const auto& data_source: m_data_sources)
    {
        m_data_source_threads.push_back(std::thread([data_source]() {
            data_source.second->runLoop();
        }));
    }
}

void GuiClient::installGuiUpdateCallback()
{
    for (const auto& data_source: m_data_sources)
    {
        if (data_source.second->getType() == "camera")
        {
            installFlipFrameCallbackCallback(data_source.second);
        }
    }
}

void GuiClient::installFlipFrameCallbackCallback(std::shared_ptr<DataSource> data_source)
{
    data_source->setFlipFrameCallback([this](const uint8_t* data, int width, int height) {
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
    for (auto& data_source: m_data_sources)
    {
        data_source.second->stop();
    }

    for (auto& thread: m_data_source_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    m_app->quit();
}


} // namespace data_recorder
} // namespace apps
